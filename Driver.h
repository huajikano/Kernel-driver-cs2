#ifndef _DRIVER_H_
#define _DRIVER_H_

#include <ntddk.h> // DDK/WDK 核心头文件，包含大部分内核API
#include <wdf.h>   // KMDF 框架头文件
#include "Common.h" // 包含我们共享的IOCTL定义和结构体

// 避免一些WDK宏定义的重名警告
#pragma warning(disable:4201) // nonstandard extension used: nameless struct/union
#pragma warning(disable:4214) // nonstandard extension used: bit field types other than int

// 驱动全局常量
#define MY_DRIVER_TAG 'pMeM' // 用于内存分配的池标签，方便在调试器中识别

// 声明所有驱动回调函数
// 这些函数都是由WDK框架调用的，需要遵循特定的签名

// DriverEntry: 驱动入口点，当驱动加载时调用
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
);

// EvtDriverDeviceAdd: 当PnP管理器发现一个与此驱动匹配的设备时调用
NTSTATUS
MyDriverEvtDeviceAdd(
    _In_ WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
);

// EvtDriverUnload: 当驱动被卸载时调用，用于资源清理
VOID
MyDriverEvtDriverUnload(
    _In_ WDFDRIVER Driver
);

// EvtIoDeviceControl: 处理用户态发来的IOCTL请求
VOID
MyDriverEvtIoDeviceControl(
    _In_ WDFQUEUE    Queue,
    _In_ WDFREQUEST  Request,
    _In_ ULONG       OutputBufferLength,
    _In_ ULONG       InputBufferLength,
    _In_ ULONG       IoControlCode
);

#endif // _DRIVER_H_
