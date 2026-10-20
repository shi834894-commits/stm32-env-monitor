# 通用 STM32 调试初始化工具

这套脚本用于给任意 STM32 工程自动生成 VS Code 的 Cortex-Debug 配置。

## 当前工程初始化

在工程根目录运行：

```powershell
.\tools\stm32-debug\setup.ps1
```

脚本会自动：

1. 查找 `.ioc` 文件；
2. 从 `ProjectManager.DeviceId` 读取芯片型号；
3. 查找 `build`、`Debug` 或 `Release` 目录中的 `.elf`；
4. 生成 `.vscode\launch.json`；
5. 写入两个调试入口：自动启动 ST-LINK Server、连接外部 ST-LINK Server。

如果工程没有标准文件名，可以手动指定：

```powershell
.\tools\stm32-debug\setup.ps1 `
    -IocPath .\board.ioc `
    -ElfPath .\build\Debug\firmware.elf `
    -Device STM32F407VG
```

如果当前工程已经有 `launch.json`，脚本默认拒绝覆盖。确认要覆盖时才使用：

```powershell
.\tools\stm32-debug\setup.ps1 -Force
```

## 工具链环境变量

生成的 `launch.json` 使用以下两个环境变量，因此不同电脑不需要修改调试配置：

```text
STM32_STLINK_GDBSERVER=ST-LINK_gdbserver.exe 的完整路径
STM32_GDB=arm-none-eabi-gdb.exe 的完整路径
```

也可以只在当前 PowerShell 窗口临时设置：

```powershell
$env:STM32_STLINK_GDBSERVER = 'C:\path\to\ST-LINK_gdbserver.exe'
$env:STM32_GDB = 'C:\path\to\arm-none-eabi-gdb.exe'
```

## 复用到其他工程

把 `tools\stm32-debug` 文件夹复制到其他工程，然后在那个工程根目录运行同一个 `setup.ps1` 即可。脚本会根据新工程的 `.ioc` 和 `.elf` 自动生成对应配置。

如果没有找到 `.elf`，先编译工程再运行初始化脚本。
