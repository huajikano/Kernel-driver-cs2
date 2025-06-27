#include <iostream>
#include <Windows.h>
#include <string>
#include <vector>
#include <TlHelp32.h> // For getting process ID
#include "Common.h"   // 包含共享的IOCTL定义和结构体

// 函数声明
HANDLE OpenDriver();
BOOL ReadProcessMemoryDriver(HANDLE hDriver, ULONG ProcessId, ULONG64 Address, PVOID Buffer, SIZE_T Size, SIZE_T* BytesRead);
BOOL WriteProcessMemoryDriver(HANDLE hDriver, ULONG ProcessId, ULONG64 Address, PVOID Buffer, SIZE_T Size, SIZE_T* BytesWritten);
ULONG GetProcessIdByName(const wchar_t* ProcessName);

int main() {
    std::wcout << L"*** 用户态进程内存读写测试程序 ***" << std::endl;

    // 1. 打开驱动句柄
    HANDLE hDriver = OpenDriver();
    if (hDriver == INVALID_HANDLE_VALUE) {
        std::wcout << L"[-] 无法打开驱动，请确保驱动已加载且测试签名模式已开启。" << std::endl;
        std::wcout << L"提示：bcdedit /set testsigning on && reboot" << std::endl;
        system("pause");
        return 1;
    }
    std::wcout << L"[+] 成功获取驱动句柄: 0x" << hDriver << std::endl;

    // 2. 获取目标进程PID (以 Notepad.exe 为例)
    ULONG targetPid = GetProcessIdByName(L"notepad.exe");
    if (targetPid == 0) {
        std::wcout << L"[-] 未找到 Notepad.exe 进程。请先打开一个 Notepad 实例。" << std::endl;
        CloseHandle(hDriver);
        system("pause");
        return 1;
    }
    std::wcout << L"[+] 已找到 Notepad.exe，PID: " << targetPid << std::endl;

    // 3. 演示内存写入
    // 注意：这里需要找到Notepad进程中一个可写入的地址。
    // 通常是堆内存或可写的数据段。为了演示，我们假设在Notepad中分配一块内存
    // 或找到一个可写的字符串。实际应用中，你需要通过其他方式获取到有效地址。
    // 这里我们尝试写入Notepad.exe进程的PEB基址（通常不可写），这仅为演示MmCopyVirtualMemory的调用，
    // 实际写入通常是对进程堆或数据段。
    // **警告：直接写入任意地址可能导致目标进程崩溃或不稳定。**
    // 假设我们找一个字符串来写入，但为了简单，我们尝试向notepad的某个已知地址写入一个int值
    // 正常来说，这里需要一个有效的、可写入的地址。
    // 例如，你可以注入一个DLL到目标进程，然后在DLL中分配内存并获取其地址，再通过驱动读写。
    // 或者利用一些已知漏洞或特性来获取可写地址。
    // 这里我们为了演示驱动功能，将尝试写入一个非常规地址，但您需要替换为实际可写的地址。

    // 为了简单起见，我们假定目标进程有一个我们想操作的特定地址。
    // 实际逆向中，您会通过扫描内存或分析结构来找到这些地址。
    // 假设我们要写一个整数到 Notepad 进程的某个地址 0x7FF700001000 (这只是一个示例地址，实际需要查找)
    // 这是一个高风险操作，因为这个地址可能无效或不可写。
    // 更安全的做法是，在目标进程中找到一个已有的可写变量或在调试器中分配内存并记录地址。
    // 例如，如果Notepad分配了一块堆内存，我们可以尝试读写那块内存。

    // 为了让示例更"安全"一点，我们假设目标进程有一个全局变量或者静态字符串，我们可以尝试读写它。
    // 但那需要知道其准确地址，这超出了一个通用示例的范围。
    // 所以，我们用一个**概念性地址**进行读写演示，请**务必**在实际操作中替换为有效地址！

    std::wcout << L"\n--- 内存读写演示 ---" << std::endl;
    ULONG64 testAddress = 0x7FF700001000; // ⚠️ 警告: 这是一个示例地址，请替换为有效的、可写入的地址！

    // 尝试写入一个整数
    int valueToWrite = 0xDEADBEEF;
    SIZE_T bytesWritten = 0;
    std::wcout << L"[*] 尝试向 PID " << targetPid << L" 的地址 0x" << std::hex << testAddress << L" 写入 0x" << valueToWrite << L" (4字节)..." << std::endl;
    if (WriteProcessMemoryDriver(hDriver, targetPid, testAddress, &valueToWrite, sizeof(valueToWrite), &bytesWritten)) {
        std::wcout << L"[+] 写入成功，写入了 " << std::dec << bytesWritten << L" 字节。" << std::endl;
    } else {
        std::wcout << L"[-] 写入失败，错误码: " << GetLastError() << std::endl;
    }

    // 4. 演示内存读取
    // 尝试读取刚才写入的整数
    int readValue = 0;
    SIZE_T bytesRead = 0;
    std::wcout << L"[*] 尝试从 PID " << targetPid << L" 的地址 0x" << std::hex << testAddress << L" 读取 4 字节..." << std::endl;
    if (ReadProcessMemoryDriver(hDriver, targetPid, testAddress, &readValue, sizeof(readValue), &bytesRead)) {
        std::wcout << L"[+] 读取成功，读取了 " << std::dec << bytesRead << L" 字节。读取到的值为: 0x" << std::hex << readValue << std::endl;
    } else {
        std::wcout << L"[-] 读取失败，错误码: " << GetLastError() << std::endl;
    }

    // 5. 关闭驱动句柄
    CloseHandle(hDriver);
    std::wcout << L"[+] 驱动句柄已关闭。" << std::endl;

    std::wcout << L"\n测试完成。按任意键退出..." << std::endl;
    system("pause");
    return 0;
}

// 打开驱动设备句柄
HANDLE OpenDriver() {
    // CreateFile 的第一个参数是驱动的符号链接名称
    return CreateFileW(DRIVER_SYMBOLIC_LINK,
                       GENERIC_READ | GENERIC_WRITE, // 读写权限
                       0,                            // 不共享
                       NULL,                         // 默认安全属性
                       OPEN_EXISTING,                // 必须已存在
                       FILE_ATTRIBUTE_NORMAL,        // 普通文件属性
                       NULL);                        // 无模板文件
}

// 通过驱动读取进程内存
BOOL ReadProcessMemoryDriver(HANDLE hDriver, ULONG ProcessId, ULONG64 Address, PVOID Buffer, SIZE_T Size, SIZE_T* BytesRead) {
    if (!hDriver || hDriver == INVALID_HANDLE_VALUE || !Buffer || Size == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    MEM_OPERATION_REQUEST request = { 0 };
    request.ProcessId = ProcessId;
    request.TargetAddress = Address;
    request.Size = Size;

    DWORD returnedBytes = 0;
    // DeviceIoControl:
    // hDevice: 驱动句柄
    // dwIoControlCode: IOCTL代码
    // lpInBuffer: 指向 MEM_OPERATION_REQUEST 的指针
    // nInBufferSize: MEM_OPERATION_REQUEST 的大小
    // lpOutBuffer: 指向接收数据的缓冲区的指针 (与 lpInBuffer 相同，因为是 METHOD_BUFFERED)
    // nOutBufferSize: 接收数据缓冲区的大小
    // lpBytesReturned: 实际返回的字节数
    // lpOverlapped: 异步I/O（这里为NULL，同步I/O）
    BOOL success = DeviceIoControl(hDriver,
                                   IOCTL_READ_PROCESS_MEMORY,
                                   &request,                     // 输入缓冲区：请求结构体
                                   sizeof(request),              // 输入缓冲区大小
                                   Buffer,                       // 输出缓冲区：接收读取到的数据
                                   (DWORD)Size,                  // 输出缓冲区大小
                                   &returnedBytes,               // 实际返回字节数
                                   NULL);

    if (BytesRead) {
        *BytesRead = success ? returnedBytes : 0;
    }
    return success;
}

// 通过驱动写入进程内存
BOOL WriteProcessMemoryDriver(HANDLE hDriver, ULONG ProcessId, ULONG64 Address, PVOID Buffer, SIZE_T Size, SIZE_T* BytesWritten) {
    if (!hDriver || hDriver == INVALID_HANDLE_VALUE || !Buffer || Size == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    MEM_OPERATION_REQUEST request = { 0 };
    request.ProcessId = ProcessId;
    request.TargetAddress = Address;
    request.Size = Size;

    // 对于写入操作，我们通过 lpInBuffer 传递 MEM_OPERATION_REQUEST，
    // 通过 lpOutBuffer 传递实际要写入的数据（因为 METHOD_BUFFERED 下 input/output buffer是同一个）
    // 所以这里的 lpInBuffer 实际上是包含 request 和 data 的复合结构，但为了简化，
    // 我们将 request 作为 input，实际数据放在 output buffer (即 DeviceIoControl 的 lpOutBuffer)
    // 在内核侧，outBuffer就是用户态传来的数据。
    
    DWORD returnedBytes = 0;
    BOOL success = DeviceIoControl(hDriver,
                                   IOCTL_WRITE_PROCESS_MEMORY,
                                   &request,                     // 输入缓冲区：请求结构体
                                   sizeof(request),              // 输入缓冲区大小
                                   Buffer,                       // 输出缓冲区：要写入的数据
                                   (DWORD)Size,                  // 输出缓冲区大小
                                   &returnedBytes,               // 实际写入字节数
                                   NULL);

    if (BytesWritten) {
        *BytesWritten = success ? returnedBytes : 0;
    }
    return success;
}

// 根据进程名称获取PID
ULONG GetProcessIdByName(const wchar_t* ProcessName) {
    ULONG pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_PROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &pe32)) {
        do {
            if (_wcsicmp(pe32.szExeFile, ProcessName) == 0) {
                pid = pe32.th32ProcessID;
                break;
            }
        } while (Process32Next(snapshot, &pe32));
    }

    CloseHandle(snapshot);
    return pid;
}
