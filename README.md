# Kernel-driver-cs2
Kernel driver cs2
📝 构建与测试指南 📝
1. 内核驱动（.sys 文件）构建
Visual Studio 配置:

打开 Visual Studio (建议 2019/2022)。

确保您已安装 Windows Driver Kit (WDK)，并且其版本与您的 Visual Studio 兼容。

新建项目，选择模板 "Kernel Mode Driver, Empty (KMDF)" 或 **"Kernel Mode Driver (UMDF V2)"Kernel Mode Driver (UMDF V2) / (KMDF)"，具体名称可能因 WDK 版本略有不同。

将 Driver.h, `Driver.cDriver.c, Common.h 文件添加到项目中。

在项目属性中，确保配置为 Release / x64（或 Debug / x64 用于调试）。

构建项目: 编译成功后，您会在项目目录的 x64\Release (或 Debug) 文件夹

打开 Visual Studio (建议 2019/2022)。

确保您已安装 Windows Driver Kit (WDK)，并且其版本与您的 Visual Studio 兼容。

新建项目，选择模板 "Kernel Mode Driver, Empty (KMDF)" 或 "Kernel Mode Driver (UMDF V2) / (KMDF)"，具体名称可能因 WDK 版本略有不同。

将 Driver.h, Driver.c, Common.h 文件添加到项目中。

在项目属性中，确保配置为 Release / x64（或 Debug / x64 用于调试）。

构建项目: 编译成功后，您会在项目目录的 x64\Release (或 Debug) 文件夹下找到 MyProcessMemoryDriver.sys 文件。

部署到测试机:

将 `MyProcessMemoryDriver.MyProcessMemoryDriver.sys 文件复制到您的 Windows 10 22H2 虚拟机或测试机器上。建议放在一个容易找到的目录，例如 C:\Drivers\。

启用测试签名模式:

在 测试机 上，以管理员身份打开命令提示符 (CMD)。

运行命令：bcdedit /set testsigning on

重启测试机。这是因为 64 位 Windows 默认只允许加载微软签名的驱动。

加载驱动:

在测试机上，以管理员身份打开命令提示符。

使用 sc 命令创建并启动服务：

sc create MyProcessMemoryDriver binPath= C:\Drivers\MyProcessMemory
```Driver.sys type= kernel
sc start MyProcessMemoryDriver

如果一切顺利，您会看到服务创建和启动成功的消息。如果出现错误，请检查路径、驱动签名或调试器输出。

2. 用户态应用程序（.exe 文件）构建
Visual Studio 配置:

新建一个 C++ Console App 项目。

将 UserApp.cpp 和 `Common.Common.h 文件添加到项目中。

确保项目配置为 Release / x64。

构建项目: 编译成功

新建一个 C++ Console App 项目。

将 UserApp.cpp 和 Common.h 文件添加到项目中。

确保项目配置为 Release / x64。

构建项目: 编译成功后，您会在项目目录的 x64\Release 文件夹下找到 UserApp.exe。

**运行测试运行测试:

将 UserApp.exe 复制到测试机上（与驱动文件放在一起或任意位置）。

先打开一个 Notepad.exe 进程，以便测试程序可以找到它。

以管理员身份运行 `UserAppUserApp.exe。

观察控制台输出。如果驱动加载成功且操作无误，您应该能看到内存读写成功的消息。如果失败，会显示相应的错误码。

3. 调试
内核调试: 配置 WinDbg 与您的虚拟机进行内核调试（推荐使用网络调试）。当驱动加载失败或蓝屏时，WinDbg 是分析问题的关键。

DbgPrint: 内核驱动中的 DbgPrint 宏会将调试信息输出到内核调试器中。请务必连接 WinDbg 查看这些输出。

⚠️ 极度重要的安全与稳定性提示 ⚠️
特权级别: 这个驱动运行在最高特权级 (Ring 0)。任何微小的编程错误都可能导致系统蓝屏 (BSOD)。请务必小心。

测试环境: **切勿在生产环境或切勿在生产环境或日常使用的机器上测试此驱动！ 始终在隔离的虚拟机或专用测试机上进行开发和测试。

MmCopyVirtualMemory: 虽然 MmCopyVirtualMemory 是安全的内存拷贝函数，但您提供的 TargetAddress 必须是目标进程中有效的、可访问的虚拟内存地址。尝试读写无效或受保护的内存区域仍然会导致蓝屏或未定义行为。在实际逆向工程中，您需要精确计算目标地址。

进程保护: 某些进程（例如，受 PPL - Protected Process Light 保护的系统关键进程或反作弊进程）可能仍然难以通过这种直接方式读写，即使在内核态也可能需要更高级的技术来绕过其保护机制。

测试签名: 完成开发后，为了系统安全，请务必在测试机上关闭测试签名模式：bcdedit /set testsigning off，并重启。

这个项目是您深入 Windows 内核世界的绝佳起点。它涵盖了驱动开发、IOCTL通信以及核心内存操作的关键概念。祝您一切顺利！👍
