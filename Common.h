#ifndef _COMMON_H_
#define _COMMON_H_

#include <Windows.h>

// --- 定义用于用户态和内核态通信的IOCTL代码 ---
// 读写单个地址的IOCTL (保持不变)
#define IOCTL_READ_PROCESS_MEMORY \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_WRITE_PROCESS_MEMORY \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

// 新增IOCTL：读取CS2游戏数据
#define IOCTL_READ_CS2_GAME_DATA \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

// --- 定义用户态请求和内核返回的数据结构 ---
// 用于读写单个地址的请求结构 (保持不变)
typedef struct _MEM_OPERATION_REQUEST {
    ULONG ProcessId;
    ULONGLONG TargetAddress;
    SIZE_T Size;
} MEM_OPERATION_REQUEST, *PMEM_OPERATION_REQUEST;

// 用于传输玩家坐标和血量数据的结构体
typedef struct _PLAYER_DATA {
    float x;
    float y;
    float z;
    int Health;
} PLAYER_DATA, *PPLAYER_DATA;

// 用于传输所有游戏数据的响应结构体
// 这个结构体将被内核驱动填充，然后拷贝回用户态
#define MAX_PLAYERS 10 // 1个本地玩家 + 9个其他玩家
typedef struct _CS2_GAME_DATA {
    // 玩家数据数组 (索引0为本地玩家，1-9为其他玩家)
    PLAYER_DATA Players[MAX_PLAYERS];
    // FOV 数据
    float FovX;
    float FovY;
    // 错误状态，如果读取失败可以返回给用户态
    NTSTATUS Status;
} CS2_GAME_DATA, *PCS2_GAME_DATA;

// 驱动设备名称和符号链接名称 (保持不变)
#define DRIVER_DEVICE_NAME  L"\\Device\\MyProcessMemoryDriver"
#define DRIVER_SYMBOLIC_LINK L"\\DosDevices\\MyProcessMemoryDriverLink"

#endif // _COMMON_H_
