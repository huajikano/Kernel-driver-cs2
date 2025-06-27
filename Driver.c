#include "Driver.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, MyDriverEvtDeviceAdd)
#pragma alloc_text(PAGE, MyDriverEvtDriverUnload)
#pragma alloc_text(PAGE, MyDriverEvtIoDeviceControl)
#pragma alloc_text(PAGE, GetModuleBaseAddress)
#endif

// 用于PEB/LDR解析的未公开结构体定义
// ⚠️ 注意：这些结构体是未文档化的，在不同Windows版本上可能存在差异！
// 但对于Windows 10 22H2及类似版本通常是稳定的。
typedef struct _PEB_LDR_DATA {
    ULONG Length;
    BOOLEAN Initialized;
    HANDLE SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
    PVOID EntryInProgress;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

typedef struct _LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
    ULONG Flags;
    USHORT LoadCount;
    USHORT TlsIndex;
    LIST_ENTRY HashLinks;
    ULONG TimeDateStamp;
} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

// 驱动入口和设备添加/卸载函数保持不变...

// DriverEntry, MyDriverEvtDeviceAdd, MyDriverEvtDriverUnload...
// ... (代码与上次提供的一致，这里省略以保持简洁) ...
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    DbgPrint("MyProcessMemoryDriver: DriverEntry called.\n");
    WDF_DRIVER_CONFIG_INIT(&config, MyDriverEvtDeviceAdd);
    config.EvtDriverUnload = MyDriverEvtDriverUnload;
    status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        DbgPrint("MyProcessMemoryDriver: WdfDriverCreate failed with status 0x%x\n", status);
    } else {
        DbgPrint("MyProcessMemoryDriver: WdfDriverCreate successful.\n");
    }
    return status;
}

NTSTATUS
MyDriverEvtDeviceAdd(
    _In_ WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    NTSTATUS status;
    WDFDEVICE device;
    WDF_IO_QUEUE_CONFIG ioQueueConfig;
    PAGED_CODE();
    DECLARE_UNICODE_STRING_SIZE(symbolicLinkName, 256);
    DbgPrint("MyProcessMemoryDriver: MyDriverEvtDeviceAdd called.\n");
    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_UNKNOWN);
    WdfDeviceInitSetIoInBandByBytes(DeviceInit);
    status = WdfDeviceCreate(&DeviceInit, WDF_NO_OBJECT_ATTRIBUTES, &device);
    if (!NT_SUCCESS(status)) {
        DbgPrint("MyProcessMemoryDriver: WdfDeviceCreate failed with status 0x%x\n", status);
        return status;
    }
    DbgPrint("MyProcessMemoryDriver: WdfDeviceCreate successful.\n");
    RtlUnicodeStringInit(&symbolicLinkName, DRIVER_SYMBOLIC_LINK);
    status = WdfDeviceCreateSymbolicLink(device, &symbolicLinkName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("MyProcessMemoryDriver: WdfDeviceCreateSymbolicLink failed with status 0x%x\n", status);
        return status;
    }
    DbgPrint("MyProcessMemoryDriver: WdfDeviceCreateSymbolicLink successful: %wZ\n", &symbolicLinkName);
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig, WdfIoQueueDispatchParallel);
    ioQueueConfig.EvtIoDeviceControl = MyDriverEvtIoDeviceControl;
    status = WdfIoQueueCreate(device, &ioQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        DbgPrint("MyProcessMemoryDriver: WdfIoQueueCreate failed with status 0x%x\n", status);
        return status;
    }
    DbgPrint("MyProcessMemoryDriver: WdfIoQueueCreate successful.\n");
    return status;
}

VOID
MyDriverEvtDriverUnload(
    _In_ WDFDRIVER Driver
)
{
    UNREFERENCED_PARAMETER(Driver);
    PAGED_CODE();
    DbgPrint("MyProcessMemoryDriver: MyDriverEvtDriverUnload called. Driver is unloading.\n");
}

// ====================================================================================
// 核心新增函数：在进程中查找指定模块的基址
// ====================================================================================
PVOID
GetModuleBaseAddress(
    _In_ PEPROCESS Process,
    _In_ PCWSTR ModuleName
)
{
    PPEB pPeb = PsGetProcessPeb(Process);
    PAGED_CODE();

    if (!pPeb) {
        DbgPrint("MyProcessMemoryDriver: Failed to get PEB for process.\n");
        return NULL;
    }

    // 附加到目标进程上下文以安全访问PEB
    KAPC_STATE apcState;
    KeStackAttachProcess(Process, &apcState);

    // 访问PEB的Ldr成员，获取模块列表
    PPEB_LDR_DATA pLdr = (PPEB_LDR_DATA)pPeb->Ldr;
    if (!pLdr || !pLdr->InLoadOrderModuleList.Flink) {
        DbgPrint("MyProcessMemoryDriver: Failed to get LDR data or module list.\n");
        KeUnstackDetachProcess(&apcState);
        return NULL;
    }

    // 遍历InLoadOrderModuleList链表
    PLIST_ENTRY pListEntry = pLdr->InLoadOrderModuleList.Flink;
    while (pListEntry != &pLdr->InLoadOrderModuleList) {
        // 通过CONTAINING_RECORD宏，从LIST_ENTRY获取LDR_DATA_TABLE_ENTRY的地址
        PLDR_DATA_TABLE_ENTRY pEntry = CONTAINING_RECORD(pListEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

        // 检查模块名称是否匹配
        if (pEntry->BaseDllName.Buffer && pEntry->BaseDllName.Length > 0) {
            if (_wcsicmp(pEntry->BaseDllName.Buffer, ModuleName) == 0) {
                DbgPrint("MyProcessMemoryDriver: Found module %wZ at 0x%p\n", &pEntry->BaseDllName, pEntry->DllBase);
                // 找到后，分离上下文并返回基址
                KeUnstackDetachProcess(&apcState);
                return pEntry->DllBase;
            }
        }
        // 移动到下一个链表项
        pListEntry = pListEntry->Flink;
    }

    // 如果遍历完没有找到，分离上下文并返回NULL
    KeUnstackDetachProcess(&apcState);
    DbgPrint("MyProcessMemoryDriver: Module %ws not found in process.\n", ModuleName);
    return NULL;
}

// ====================================================================================
// EvtIoDeviceControl - IOCTL处理函数 (更新)
// ====================================================================================
VOID
MyDriverEvtIoDeviceControl(
    _In_ WDFQUEUE    Queue,
    _In_ WDFREQUEST  Request,
    _In_ ULONG       OutputBufferLength,
    _In_ ULONG       InputBufferLength,
    _In_ ULONG       IoControlCode
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PVOID inBuffer = NULL;
    PVOID outBuffer = NULL;
    SIZE_T inBufferSize = 0;
    SIZE_T outBufferSize = 0;
    PEPROCESS targetEProcess = NULL;
    KAPC_STATE apcState;
    SIZE_T bytesCopied = 0;

    PAGED_CODE();
    UNREFERENCED_PARAMETER(Queue);

    DbgPrint("MyProcessMemoryDriver: EvtIoDeviceControl received IOCTL: 0x%x\n", IoControlCode);

    // 获取输入/输出缓冲区
    status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &inBuffer, &inBufferSize);
    if (!NT_SUCCESS(status)) goto CompleteRequest;
    status = WdfRequestRetrieveOutputBuffer(Request, OutputBufferLength, &outBuffer, &outBufferSize);
    if (!NT_SUCCESS(status)) goto CompleteRequest;

    // 根据IOCTL代码分派处理
    switch (IoControlCode) {
        // --- 通用读写逻辑 (保持不变) ---
        case IOCTL_READ_PROCESS_MEMORY:
        case IOCTL_WRITE_PROCESS_MEMORY: {
            PMEM_OPERATION_REQUEST pRequest = (PMEM_OPERATION_REQUEST)inBuffer;
            // 确保输入缓冲区足够包含请求结构
            if (inBufferSize < sizeof(MEM_OPERATION_REQUEST)) {
                status = STATUS_INFO_LENGTH_MISMATCH;
                DbgPrint("MyProcessMemoryDriver: Buffer size mismatch for generic RW.\n");
                goto CompleteRequest;
            }

            status = PsLookupProcessByProcessId(UlongToHandle(pRequest->ProcessId), &targetEProcess);
            if (!NT_SUCCESS(status)) goto CompleteRequest;
            
            KeStackAttachProcess(targetEProcess, &apcState);

            if (IoControlCode == IOCTL_READ_PROCESS_MEMORY) {
                if (outBufferSize < pRequest->Size) { status = STATUS_BUFFER_TOO_SMALL; goto DetachAndDereference; }
                status = MmCopyVirtualMemory(targetEProcess, (PVOID)pRequest->TargetAddress, PsGetCurrentProcess(), outBuffer, pRequest->Size, KernelMode, &bytesCopied);
            } else { // IOCTL_WRITE_PROCESS_MEMORY
                if (outBufferSize < pRequest->Size) { status = STATUS_BUFFER_TOO_SMALL; goto DetachAndDereference; }
                status = MmCopyVirtualMemory(PsGetCurrentProcess(), outBuffer, targetEProcess, (PVOID)pRequest->TargetAddress, pRequest->Size, KernelMode, &bytesCopied);
            }

        DetachAndDereference:
            KeUnstackDetachProcess(&apcState);
            ObDereferenceObject(targetEProcess);
            break;
        }

        // --- 新增：读取CS2游戏数据 ---
        case IOCTL_READ_CS2_GAME_DATA: {
            // 确保输出缓冲区足够大，可以容纳CS2_GAME_DATA结构
            if (outBufferSize < sizeof(CS2_GAME_DATA)) {
                status = STATUS_BUFFER_TOO_SMALL;
                DbgPrint("MyProcessMemoryDriver: Output buffer too small for CS2_GAME_DATA.\n");
                goto CompleteRequest;
            }

            PCS2_GAME_DATA pGameData = (PCS2_GAME_DATA)outBuffer;
            RtlZeroMemory(pGameData, sizeof(CS2_GAME_DATA)); // 清空结构体
            pGameData->Status = STATUS_SUCCESS; // 默认成功

            // 1. 从用户态获取PID (假设用户态在inBuffer中传入PID)
            ULONG targetPid = *(PULONG)inBuffer;
            if (inBufferSize < sizeof(ULONG)) {
                status = STATUS_INFO_LENGTH_MISMATCH;
                DbgPrint("MyProcessMemoryDriver: Input buffer for PID is too small.\n");
                goto CompleteRequest;
            }

            // 2. 查找目标进程
            status = PsLookupProcessByProcessId(UlongToHandle(targetPid), &targetEProcess);
            if (!NT_SUCCESS(status)) {
                pGameData->Status = status; // 将错误状态传回用户态
                DbgPrint("MyProcessMemoryDriver: Failed to find process with PID %lu.\n", targetPid);
                goto CompleteRequest;
            }

            // 3. 查找 client.dll 的基址
            PVOID clientDllBase = GetModuleBaseAddress(targetEProcess, L"client.dll");
            if (!clientDllBase) {
                pGameData->Status = STATUS_NOT_FOUND;
                DbgPrint("MyProcessMemoryDriver: client.dll not found in process.\n");
                ObDereferenceObject(targetEProcess);
                goto CompleteRequest;
            }

            // 4. 附加到目标进程上下文
            KeStackAttachProcess(targetEProcess, &apcState);

            // 5. 计算基址和偏移，并读取数据
            // 玩家数组首地址: client.dll + 189A268
            ULONGLONG playerArrayBaseAddress = (ULONGLONG)clientDllBase + 0x189A268;
            ULONGLONG localPlayerAddress = 0;
            
            // 读取本地玩家指针
            status = MmCopyVirtualMemory(targetEProcess, (PVOID)(playerArrayBaseAddress + 8), PsGetCurrentProcess(), &localPlayerAddress, sizeof(ULONGLONG), KernelMode, NULL);
            if (!NT_SUCCESS(status) || localPlayerAddress == 0) {
                pGameData->Status = STATUS_NO_DATA_DETECTED;
                DbgPrint("MyProcessMemoryDriver: Failed to read local player address or it's null.\n");
                goto DetachAndDereferenceGameData;
            }

            // 读取本地玩家数据 (索引0)
            status = MmCopyVirtualMemory(targetEProcess, (PVOID)(localPlayerAddress + 0xDB8), PsGetCurrentProcess(), &pGameData->Players[0].x, sizeof(float), KernelMode, NULL);
            status |= MmCopyVirtualMemory(targetEProcess, (PVOID)(localPlayerAddress + 0xDBC), PsGetCurrentProcess(), &pGameData->Players[0].y, sizeof(float), KernelMode, NULL);
            status |= MmCopyVirtualMemory(targetEProcess, (PVOID)(localPlayerAddress + 0xDC0), PsGetCurrentProcess(), &pGameData->Players[0].z, sizeof(float), KernelMode, NULL);
            // 血量偏移
            status |= MmCopyVirtualMemory(targetEProcess, (PVOID)(localPlayerAddress + 0x344), PsGetCurrentProcess(), &pGameData->Players[0].Health, sizeof(int), KernelMode, NULL);
            if (!NT_SUCCESS(status)) DbgPrint("MyProcessMemoryDriver: Failed to read local player data.\n");

            // 读取其他9个玩家数据 (索引1-9)
            for (int i = 1; i < MAX_PLAYERS; ++i) {
                ULONGLONG otherPlayerAddress = 0;
                // 其他玩家地址: 玩家数组首地址 + 10h * i
                status = MmCopyVirtualMemory(targetEProcess, (PVOID)(playerArrayBaseAddress + (0x10 * i)), PsGetCurrentProcess(), &otherPlayerAddress, sizeof(ULONGLONG), KernelMode, NULL);
                if (!NT_SUCCESS(status) || otherPlayerAddress == 0) continue; // 如果读取失败或地址为空，跳过

                // 读取其他玩家的坐标和血量
                status = MmCopyVirtualMemory(targetEProcess, (PVOID)(otherPlayerAddress + 0xDB8), PsGetCurrentProcess(), &pGameData->Players[i].x, sizeof(float), KernelMode, NULL);
                status |= MmCopyVirtualMemory(targetEProcess, (PVOID)(otherPlayerAddress + 0xDBC), PsGetCurrentProcess(), &pGameData->Players[i].y, sizeof(float), KernelMode, NULL);
                status |= MmCopyVirtualMemory(targetEProcess, (PVOID)(otherPlayerAddress + 0xDC0), PsGetCurrentProcess(), &pGameData->Players[i].z, sizeof(float), KernelMode, NULL);
                status |= MmCopyVirtualMemory(targetEProcess, (PVOID)(otherPlayerAddress + 0x344), PsGetCurrentProcess(), &pGameData->Players[i].Health, sizeof(int), KernelMode, NULL);
                if (!NT_SUCCESS(status)) DbgPrint("MyProcessMemoryDriver: Failed to read player %d data.\n", i);
            }

            // 读取 FOV 值
            ULONGLONG fovYAddress = (ULONGLONG)clientDllBase + 0x1A88548;
            ULONGLONG fovXAddress = (ULONGLONG)clientDllBase + 0x1A8854c;
            status = MmCopyVirtualMemory(targetEProcess, (PVOID)fovYAddress, PsGetCurrentProcess(), &pGameData->FovY, sizeof(float), KernelMode, NULL);
            status |= MmCopyVirtualMemory(targetEProcess, (PVOID)fovXAddress, PsGetCurrentProcess(), &pGameData->FovX, sizeof(float), KernelMode, NULL);
            if (!NT_SUCCESS(status)) DbgPrint("MyProcessMemoryDriver: Failed to read FOV data.\n");

        DetachAndDereferenceGameData:
            // 6. 分离上下文并减少引用计数
            KeUnstackDetachProcess(&apcState);
            ObDereferenceObject(targetEProcess);
            bytesCopied = sizeof(CS2_GAME_DATA); // 告诉用户态返回了多少字节
            break;
        }

        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            DbgPrint("MyProcessMemoryDriver: Received unknown IOCTL: 0x%x\n", IoControlCode);
            break;
    }

CompleteRequest:
    if (NT_SUCCESS(status)) {
        WdfRequestSetInformation(Request, bytesCopied);
    }
    WdfRequestComplete(Request, status);
}
