# OLED / FreeRTOS 故障排查交接

目标项目：STM32 + FreeRTOS 工业数据采集与 Modbus 通信终端

## 故障现象

此前版本可显示 AHT20 温湿度；更新代码后 OLED 停留在旧数据，随后断电重启变为黑屏。串口日志曾停留在 OLED 初始化附近，因此排查重点放在启动阶段的延时、软件 I2C 和 RTOS 任务启动，而不是传感器硬件本身。

## 已做处理

- OLED 上电等待从 `HAL_Delay(100)` 改为独立忙等，避免调度器启动前 HAL tick 不推进时卡在初始化阶段。
- AHT20 延时改为：调度器运行后用 `vTaskDelay()`，调度器运行前用忙等。
- FreeRTOS 动态堆从 5120 增至 10240 字节，避免创建队列、互斥锁或任务时因堆不足失败。
- 增加 OLED 初始化失败重试、任务启动标记，以及 I2C 互斥锁保护和刷新失败后的总线重新初始化逻辑。
- OLED/传感器初始化失败不再直接进入 `Error_Handler()`，以便任务运行后重试。

涉及文件：`Core/Inc/FreeRTOSConfig.h`、`BSP/Src/bsp_oled.c`、`BSP/Src/bsp_aht20.c`、`App/Src/app_main.c`。此前 I2C、日志和 `Core/Src/main.c` 也有本地修改，接手时应先检查工作区，不要覆盖或回滚。

## 已验证情况

- Debug 构建成功，且此前已烧录并校验到已连接的板子。
- 串口抓取记录显示 USART1 logger、软件 I2C、OLED 初始化、两个 RTOS 任务均启动；AHT20 温湿度连续变化；DisplayTask 持续经过 page 0 至 page 7 刷新路径。日志记录位于 `tools/runtime-capture.txt` 和 `tools/serial-capture.txt`。
- Release 构建成功；Release 烧录及运行结果尚未确认。
- 上述日志只能证明固件执行到了 OLED 刷新代码路径，不能证明面板实际收到了 I2C 数据或已经点亮。当前 `BSP/Src/bsp_oled.c` 的写命令/数据函数丢弃 ACK，`OLED_Init()` 也无条件返回成功；要确认屏幕问题已解决，必须恢复 ACK/NACK 检查或用逻辑分析仪/屏幕实测确认。

## 串口查看

仓库提供 `tools/putty.exe` 和 `tools/open-serial-log.ps1`。默认脚本参数为 COM4、115200、8N1；可运行：

```powershell
.\tools\open-serial-log.ps1 -Port COM4 -Baud 115200
```

## 接手后的优先事项

1. 检查当前 I2C 软件模拟实现的时序、GPIO 开漏与上拉配置，并恢复 OLED 命令/数据传输的 ACK 检查；探测常见地址 `0x3C` / `0x3D`。
2. 让 `OLED_Init()` 只在实际收到 ACK 后报告成功，刷新失败时记录页号/阶段，避免“日志成功但屏幕黑屏”的假阳性。
3. 烧录 Debug 固件后验证 OLED 实际显示；再连续观察温湿度是否更新。完成后再决定是否验证 Release 固件。

## 诊断结论边界

软件 I2C 与 USART 日志放在 FreeRTOS 中本身不会必然堵塞 CPU；同步 UART 打印会占用时间，软件 I2C 会忙等并占用执行时间，但本次采集日志显示两个任务能运行。当前更需要排除 I2C ACK/地址/时序以及驱动忽略错误造成的假成功。原始卡死也可能与启动路径里的 HAL 延时不推进有关；应以 ACK 检查恢复后的硬件实测作为最终结论。
