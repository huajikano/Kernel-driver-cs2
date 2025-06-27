#ifndef _DRIVER_H_
#define _DRIVER_H_

#include <ntddk.h>
#include <wdf.h>
#include "Common.h"

#pragma warning(disable:4201)
#pragma warning(disable:4214)

#define MY_DRIVER_TAG 'pMeM'

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
);

NTSTATUS
MyDriverEvtDeviceAdd(
    _In_ WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
);

VOID
MyDriverEvtDriverUnload(
    _In_ WDFDRIVER Driver
);

VOID
MyDriverEvtIoDeviceControl(
    _In_ WDFQUEUE    Queue,
    _In_ WDFREQUEST  Request,
    _In_ ULONG       OutputBufferLength,
    _In_ ULONG       InputBufferLength,
    _In_ ULONG       IoControlCode
);

// 新增函数声明：在进程中查找指定模块的基址
PVOID
GetModuleBaseAddress(
    _In_ PEPROCESS Process,
    _In_ PCWSTR ModuleName
);

#endif // _DRIVER_H_
