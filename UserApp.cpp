#include <iostream>
#include <Windows.h>
#include <string>
#include <vector>
#include <TlHelp32.h>
#include <iomanip> // For std::fixed and std::setprecision
#include "Common.h"

// 函数声明
HANDLE OpenDriver();
ULONG GetProcessIdByName(const wchar_t* ProcessName);
BOOL ReadCs2GameDataDriver(HANDLE hDriver, ULONG ProcessId, PCS2_GAME_DATA pGameData);

int main() {
    std::wcout << L"*** 用户态 CS2 内存读取测试程序 ***" << std::endl;

    // 1. 打开驱动句柄
    HANDLE hDriver = OpenDriver();
    if (hDriver == INVALID_HANDLE_VALUE) {
        std::wcout << L"[-] 无法打开驱动，请确保驱动已加载且测试签名模式已开启。" << std::endl;
        std::wcout << L"提示：bcdedit /set testsigning on && reboot" << std::endl;
        system("pause");
        return 1;
    }
    std::wcout << L"[+] 成功获取驱动句柄: 0x" << hDriver << std::endl;

    // 2. 获取目标进程PID (以 cs2.exe 为例)
    ULONG targetPid = 0;
    while (targetPid == 0) {
        targetPid = GetProcessIdByName(L"cs2.exe");
        if (targetPid == 0) {
            std::wcout << L"[*] 未找到 cs2.exe 进程。请先打开游戏，等待中..." << std::endl;
            Sleep(2000); // 等待2秒后重试
        }
    }
    std::wcout << L"[+] 已找到 cs2.exe，PID: " << targetPid << std::endl;
    std::wcout << L"\n--- 开始实时读取玩家数据 (CTRL+C 退出) ---" << std::endl;

    // 3. 循环读取数据并打印
    CS2_GAME_DATA gameData;
    while (true) {
        // 清屏
        system("cls");
        
        // 调用驱动读取数据
        if (ReadCs2GameDataDriver(hDriver, targetPid, &gameData)) {
            // 打印 FOV
            std::cout << std::fixed << std::setprecision(2);
            std::wcout << L"*** FOV: X=" << gameData.FovX << L", Y=" << gameData.FovY << L" ***" << std::endl;
            std::wcout << L"-------------------------------------" << std::endl;

            // 打印本地玩家数据
            std::wcout << L"-> 本地玩家 (Index 0):" << std::endl;
            std::wcout << L"   地址: 0x" << std::hex << (long long)&gameData.Players[0] << L", "
                       << L"坐标: X=" << gameData.Players[0].x << L", Y=" << gameData.Players[0].y << L", Z=" << gameData.Players[0].z << L", "
                       << L"血量: " << std::dec << gameData.Players[0].Health << std::endl;

            // 打印其他玩家数据
            std::wcout << L"-> 其他玩家:" << std::endl;
            for (int i = 1; i < MAX_PLAYERS; ++i) {
                if (gameData.Players[i].Health > 0) { // 只打印血量大于0的玩家
                    std::wcout << L"   玩家 #" << i << L":"
                               << L" 坐标: X=" << gameData.Players[i].x << L", Y=" << gameData.Players[i].y << L", Z=" << gameData.Players[i].z << L", "
                               << L"血量: " << std::dec << gameData.Players[i].Health << std::endl;
                }
            }
        } else {
            std::wcout << L"[-] 读取游戏数据失败，错误码: " << GetLastError() << L" | 驱动返回状态: 0x" << std::hex << gameData.Status << std::endl;
        }

        Sleep(100); // 暂停100毫秒，避免CPU占用过高
    }

    // 4. 关闭驱动句柄
    CloseHandle(hDriver);
    std::wcout << L"[+] 驱动句柄已关闭。" << std::endl;

    system("pause");
    return 0;
}

// 打开驱动设备句柄 (保持不变)
HANDLE OpenDriver() {
    return CreateFileW(DRIVER_SYMBOLIC_LINK,
                       GENERIC_READ | GENERIC_WRITE,
                       0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}

// 根据进程名称获取PID (保持不变)
ULONG GetProcessIdByName(const wchar_t* ProcessName) {
    ULONG pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_PROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
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

// 新增函数：通过驱动读取CS2游戏数据
BOOL ReadCs2GameDataDriver(HANDLE hDriver, ULONG ProcessId, PCS2_GAME_DATA pGameData) {
    if (!hDriver || hDriver == INVALID_HANDLE_VALUE || !pGameData) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    DWORD returnedBytes = 0;
    // inBuffer 只需要传递 PID
    // outBuffer 用来接收 CS2_GAME_DATA 结构
    BOOL success = DeviceIoControl(hDriver,
                                   IOCTL_READ_CS2_GAME_DATA,
                                   &ProcessId,                   // 输入缓冲区：目标PID
                                   sizeof(ProcessId),            // 输入缓冲区大小
                                   pGameData,                    // 输出缓冲区：接收数据
                                   sizeof(CS2_GAME_DATA),        // 输出缓冲区大小
                                   &returnedBytes,               // 实际返回字节数
                                   NULL);
    
    return success;
}
