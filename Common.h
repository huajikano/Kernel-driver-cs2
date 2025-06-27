#ifndef _COMMON_H_
#define _COMMON_H_

#include <Windows.h> // 包含用户态和内核态都需要的一些基本定义，例如SIZE_T, ULONG等

// 定义用于用户态和内核态通信的IOCTL代码
// CTL_CODE(DeviceType, Function, Method, Access)
// FILE_DEVICE_UNKNOWN: 通用设备类型
// 0x800, 0x801: 自定义函数码，确保不与现有系统IOCTL冲突
// METHOD_BUFFERED: I/O 缓冲区传输方法。用户态和内核态使用同一个缓冲区，系统会自动在两者之间拷贝数据。
//                  适用于小到中等大小的数据传输。
// FILE_ANY_ACCESS: 任何访问权限都可以调用此IOCTL
#define IOCTL_READ_PROCESS_MEMORY \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_WRITE_PROCESS_MEMORY \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

// 定义用户态请求内核驱动执行内存操作时传入的数据结构
// 这个结构体在用户态和内核态都必须保持一致，以确保正确的数据解析
typedef struct _MEM_OPERATION_REQUEST {
    ULONG ProcessId;       // 目标进程的PID
    ULONGLONG TargetAddress; // 目标进程中要读写内存的虚拟地址
    SIZE_T Size;           // 要读写的数据字节数
    // 注意：对于METHOD_BUFFERED，实际要读写的数据缓冲区由DeviceIoControl的lpInBuffer/lpOutBuffer参数提供
    // 而不是包含在这个结构体内部。这个结构体只包含元数据。
} MEM_OPERATION_REQUEST, *PMEM_OPERATION_REQUEST;

// 驱动设备名称和符号链接名称
// 用户态程序通过符号链接名称来打开驱动设备
#define DRIVER_DEVICE_NAME  L"\\Device\\MyProcessMemoryDriver"
#define DRIVER_SYMBOLIC_LINK L"\\DosDevices\\MyProcessMemoryDriverLink"

#endif // _COMMON_H_
