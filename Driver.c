#include "Driver.h"

#ifdef ALLOC_PRAGMA
// 告诉编译器这些函数只在可分页内存中，驱动加载后可以被换出
// DriverEntry 必须在非分页内存
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, MyDriverEvtDeviceAdd)
#pragma alloc_text(PAGE, MyDriverEvtDriverUnload)
#pragma alloc_text(PAGE, MyDriverEvtIoDeviceControl) // IOCTL处理函数通常在PASSIVE_LEVEL，可分页
#endif

// DriverEntry - 驱动的入口点
// 这是驱动加载时首先执行的函数
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;

    DbgPrint("MyProcessMemoryDriver: DriverEntry called.\n"); // 内核调试输出

    // 初始化 WDF_DRIVER_CONFIG 结构
    WDF_DRIVER_CONFIG_INIT(&config, MyDriverEvtDeviceAdd);

    // 注册驱动卸载回调函数
    config.EvtDriverUnload = MyDriverEvtDriverUnload;

    // 创建 WDFDRIVER 对象
    status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             WDF_NO_OBJECT_ATTRIBUTES, // 默认对象属性
                             &config,                  // 驱动配置
                             WDF_NO_HANDLE             // 不返回WDFDRIVER句柄，因为不需要额外引用
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("MyProcessMemoryDriver: WdfDriverCreate failed with status 0x%x\n", status);
    } else {
        DbgPrint("MyProcessMemoryDriver: WdfDriverCreate successful.\n");
    }

    return status;
}

// MyDriverEvtDeviceAdd - PnP管理器发现设备时调用
// 在这里创建设备对象和符号链接
NTSTATUS
MyDriverEvtDeviceAdd(
    _In_ WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    NTSTATUS status;
    WDFDEVICE device;
    WDF_IO_QUEUE_CONFIG ioQueueConfig;
    WDF_OBJECT_ATTRIBUTES attributes;
    PAGED_CODE(); // 标记此函数运行在可分页内存

    DECLARE_UNICODE_STRING_SIZE(deviceName, 256);
    DECLARE_UNICODE_STRING_SIZE(symbolicLinkName, 256);

    DbgPrint("MyProcessMemoryDriver: MyDriverEvtDeviceAdd called.\n");

    // 设置设备类型和特性
    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_UNKNOWN);
    WdfDeviceInitSetIoInBandByBytes(DeviceInit); // 确保在IOCTL处理中可以安全访问用户态缓冲区

    // 创建设备对象
    status = WdfDeviceCreate(&DeviceInit,
                             WDF_NO_OBJECT_ATTRIBUTES,
                             &device
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("MyProcessMemoryDriver: WdfDeviceCreate failed with status 0x%x\n", status);
        return status;
    }
    DbgPrint("MyProcessMemoryDriver: WdfDeviceCreate successful.\n");

    // 创建设备名称（可选，KMDF通常会自动生成）
    // RtlUnicodeStringInit(&deviceName, DRIVER_DEVICE_NAME);
    // status = WdfDeviceAssignDeviceName(device, &deviceName);
    // if (!NT_SUCCESS(status)) { /* handle error */ }

    // 创建符号链接，用户态应用程序将通过此名称打开设备
    RtlUnicodeStringInit(&symbolicLinkName, DRIVER_SYMBOLIC_LINK);
    status = WdfDeviceCreateSymbolicLink(device, &symbolicLinkName);

    if (!NT_SUCCESS(status)) {
        DbgPrint("MyProcessMemoryDriver: WdfDeviceCreateSymbolicLink failed with status 0x%x\n", status);
        // 如果创建符号链接失败，清理设备对象
        // WdfObjectDelete(device); // DeviceInit会自动处理
        return status;
    }
    DbgPrint("MyProcessMemoryDriver: WdfDeviceCreateSymbolicLink successful: %wZ\n", &symbolicLinkName);

    // 配置默认I/O队列
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig, WdfIoQueueDispatchParallel);

    // 注册IOCTL处理函数
    ioQueueConfig.EvtIoDeviceControl = MyDriverEvtIoDeviceControl;

    // 创建I/O队列
    status = WdfIoQueueCreate(device,
                              &ioQueueConfig,
                              WDF_NO_OBJECT_ATTRIBUTES,
                              WDF_NO_HANDLE // 不返回队列句柄，因为是默认队列
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("MyProcessMemoryDriver: WdfIoQueueCreate failed with status 0x%x\n", status);
        return status;
    }
    DbgPrint("MyProcessMemoryDriver: WdfIoQueueCreate successful.\n");

    return status;
}

// MyDriverEvtDriverUnload - 驱动卸载时调用
// 清理所有已分配的资源
VOID
MyDriverEvtDriverUnload(
    _In_ WDFDRIVER Driver
)
{
    UNREFERENCED_PARAMETER(Driver); // 避免未使用的参数警告
    PAGED_CODE(); // 标记此函数运行在可分页内存

    DbgPrint("MyProcessMemoryDriver: MyDriverEvtDriverUnload called. Driver is unloading.\n");
    // WDF 框架会自动处理设备对象和符号链接的清理
    // 无需手动删除，除非您在驱动中手动分配了额外的、不属于WDF对象树的内存
}


// MyDriverEvtIoDeviceControl - IOCTL处理函数
// 这是核心逻辑，处理用户态的读写内存请求
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
    PMEM_OPERATION_REQUEST pRequest = NULL;
    PEPROCESS targetEProcess = NULL;
    KAPC_STATE apcState; // 用于保存KeStackAttachProcess前的APC状态
    SIZE_T bytesCopied = 0;

    PAGED_CODE(); // 标记此函数运行在可分页内存

    UNREFERENCED_PARAMETER(Queue); // 避免未使用的参数警告

    DbgPrint("MyProcessMemoryDriver: EvtIoDeviceControl received IOCTL: 0x%x\n", IoControlCode);

    // 1. 获取输入/输出缓冲区
    // 对于 METHOD_BUFFERED，输入和输出缓冲区是同一个，由系统管理
    status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &inBuffer, &inBufferSize);
    if (!NT_SUCCESS(status)) {
        DbgPrint("MyProcessMemoryDriver: WdfRequestRetrieveInputBuffer failed 0x%x\n", status);
        WdfRequestComplete(Request, status);
        return;
    }

    status = WdfRequestRetrieveOutputBuffer(Request, OutputBufferLength, &outBuffer, &outBufferSize);
    if (!NT_SUCCESS(status)) {
        DbgPrint("MyProcessMemoryDriver: WdfRequestRetrieveOutputBuffer failed 0x%x\n", status);
        WdfRequestComplete(Request, status);
        return;
    }

    // 2. 检查输入缓冲区大小是否足够包含我们的请求结构体
    if (inBufferSize < sizeof(MEM_OPERATION_REQUEST)) {
        status = STATUS_INFO_LENGTH_MISMATCH; // 输入缓冲区长度不匹配
        DbgPrint("MyProcessMemoryDriver: Input buffer too small for MEM_OPERATION_REQUEST. Size: %zu, Expected: %zu\n",
                 inBufferSize, sizeof(MEM_OPERATION_REQUEST));
        WdfRequestComplete(Request, status);
        return;
    }

    pRequest = (PMEM_OPERATION_REQUEST)inBuffer; // 将输入缓冲区视为MEM_OPERATION_REQUEST结构

    // 3. 根据PID查找目标进程的EPROCESS结构体
    // PsLookupProcessByProcessId 会增加EPROCESS对象的引用计数
    status = PsLookupProcessByProcessId(UlongToHandle(pRequest->ProcessId), &targetEProcess);
    if (!NT_SUCCESS(status)) {
        DbgPrint("MyProcessMemoryDriver: PsLookupProcessByProcessId failed for PID %lu, status: 0x%x\n", pRequest->ProcessId, status);
        WdfRequestComplete(Request, status);
        return;
    }
    DbgPrint("MyProcessMemoryDriver: Found EPROCESS for PID %lu (0x%p).\n", pRequest->ProcessId, targetEProcess);

    // 4. 附加到目标进程的地址空间
    // 这会将当前线程的执行上下文切换到目标进程的虚拟地址空间
    // 允许我们使用MmCopyVirtualMemory直接访问目标进程内存
    // 在PASSIVE_LEVEL IRQL下调用KeStackAttachProcess是安全的
    KeStackAttachProcess(targetEProcess, &apcState);
    DbgPrint("MyProcessMemoryDriver: Attached to PID %lu context.\n", pRequest->ProcessId);

    // 5. 根据IOCTL代码执行读或写操作
    switch (IoControlCode) {
        case IOCTL_READ_PROCESS_MEMORY:
            DbgPrint("MyProcessMemoryDriver: IOCTL_READ_PROCESS_MEMORY - PID: %lu, Address: 0x%llx, Size: %zu\n",
                     pRequest->ProcessId, pRequest->TargetAddress, pRequest->Size);

            // 确保输出缓冲区足够大来接收数据
            if (outBufferSize < pRequest->Size) {
                status = STATUS_BUFFER_TOO_SMALL;
                DbgPrint("MyProcessMemoryDriver: Read buffer too small. Expected: %zu, Actual: %zu\n", pRequest->Size, outBufferSize);
                break;
            }

            // 核心内存读取操作
            // SourceProcess = 目标进程 (targetEProcess)
            // SourceAddress = 目标进程中的虚拟地址 (pRequest->TargetAddress)
            // TargetProcess = 当前进程 (PsGetCurrentProcess())，即用户态调用者的进程
            // TargetAddress = 用户态传入的用于接收数据的缓冲区 (outBuffer)
            status = MmCopyVirtualMemory(
                targetEProcess,                 // 源进程 (要读取内存的进程)
                (PVOID)pRequest->TargetAddress, // 源地址 (目标进程中的地址)
                PsGetCurrentProcess(),          // 目标进程 (当前发出IOCTL的进程，数据将拷贝到其用户态缓冲区)
                outBuffer,                      // 目标地址 (用户态缓冲区地址)
                pRequest->Size,                 // 要拷贝的字节数
                KernelMode,                     // 在内核模式下执行拷贝，最高权限
                &bytesCopied                    // 实际拷贝的字节数
            );

            if (NT_SUCCESS(status)) {
                DbgPrint("MyProcessMemoryDriver: Read %zu bytes successfully.\n", bytesCopied);
            } else {
                DbgPrint("MyProcessMemoryDriver: MmCopyVirtualMemory (read) failed with status 0x%x\n", status);
            }
            break;

        case IOCTL_WRITE_PROCESS_MEMORY:
            DbgPrint("MyProcessMemoryDriver: IOCTL_WRITE_PROCESS_MEMORY - PID: %lu, Address: 0x%llx, Size: %zu\n",
                     pRequest->ProcessId, pRequest->TargetAddress, pRequest->Size);

            // 确保输入缓冲区包含要写入的数据，并且大小与请求一致
            // outBuffer 实际上是 METHOD_BUFFERED 模式下的输入缓冲区，包含了待写入数据
            if (outBufferSize < pRequest->Size) { // outBuffer在此场景下是METHOD_BUFFERED的输入缓冲区
                 status = STATUS_INFO_LENGTH_MISMATCH;
                 DbgPrint("MyProcessMemoryDriver: Write buffer too small. Expected: %zu, Actual: %zu\n", pRequest->Size, outBufferSize);
                 break;
            }
            // 核心内存写入操作
            // SourceProcess = 当前进程 (PsGetCurrentProcess())，即用户态调用者的进程
            // SourceAddress = 用户态传入的待写入的数据缓冲区 (outBuffer)
            // TargetProcess = 目标进程 (targetEProcess)
            // TargetAddress = 目标进程中的虚拟地址 (pRequest->TargetAddress)
            status = MmCopyVirtualMemory(
                PsGetCurrentProcess(),          // 源进程 (数据来源于当前发出IOCTL的进程的用户态缓冲区)
                outBuffer,                      // 源地址 (用户态缓冲区地址)
                targetEProcess,                 // 目标进程 (要写入内存的进程)
                (PVOID)pRequest->TargetAddress, // 目标地址 (目标进程中的地址)
                pRequest->Size,                 // 要拷贝的字节数
                KernelMode,                     // 在内核模式下执行拷贝
                &bytesCopied                    // 实际拷贝的字节数
            );

            if (NT_SUCCESS(status)) {
                DbgPrint("MyProcessMemoryDriver: Written %zu bytes successfully.\n", bytesCopied);
            } else {
                DbgPrint("MyProcessMemoryDriver: MmCopyVirtualMemory (write) failed with status 0x%x\n", status);
            }
            break;

        default:
            status = STATUS_INVALID_DEVICE_REQUEST; // 未知IOCTL
            DbgPrint("MyProcessMemoryDriver: Received unknown IOCTL: 0x%x\n", IoControlCode);
            break;
    }

    // 6. 分离目标进程上下文
    // 无论操作成功与否，都必须分离，否则可能导致系统不稳定甚至蓝屏
    KeUnstackDetachProcess(&apcState);
    DbgPrint("MyProcessMemoryDriver: Detached from PID %lu context.\n", pRequest->ProcessId);

    // 7. 减少EPROCESS对象的引用计数
    // 对应 PsLookupProcessByProcessId 的增加引用计数操作
    ObDereferenceObject(targetEProcess);
    DbgPrint("MyProcessMemoryDriver: Dereferenced EPROCESS for PID %lu.\n", pRequest->ProcessId);

    // 8. 完成请求并返回状态
    if (NT_SUCCESS(status)) {
        WdfRequestSetInformation(Request, bytesCopied); // 告知用户态实际读/写了多少字节
    }
    WdfRequestComplete(Request, status);
}
