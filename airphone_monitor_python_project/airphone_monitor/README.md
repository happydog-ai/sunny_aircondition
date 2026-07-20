# AirPhone 暖通控制板监控程序

Python + PySide6 桌面监控程序，同时支持当前 AA55 自定义协议和后续 Modbus RTU。

## 已实现

- Windows 串口扫描、连接、断开；
- AA55：PING、GET_VERSION、ECHO、LED_SET、GET_STATUS；
- Modbus RTU：01、02、03、04、05、06、10 功能码；
- 自动轮询、寄存器换算、实时状态卡片；
- 温度曲线、执行器控制、EEPROM参数页；
- 原始 TX/RX 十六进制日志、CSV记录；
- 后台通信线程，设备超时不会阻塞GUI。

## 安装

推荐 Python 3.11/3.12：

```powershell
cd airphone_monitor
python -m venv .venv
.\.venv\Scripts\activate
python -m pip install --upgrade pip
pip install -r requirements.txt
```

## 运行

```powershell
python main.py
```

也可以双击 `run.bat`。

## 当前AA55固件

界面选择“当前 AA55 协议”，默认 COM4、9600、地址1。连接时自动读取版本，连接后自动轮询状态。

AA55帧格式：

```text
AA 55 | LEN | ADDR | SEQ | CMD | CTRL | DATA | CRC_L CRC_H
```

二进制协议不追加 `\r\n`。

## Modbus寄存器表

地址和倍率位于 `config/register_map.json`。当前内容是V1.0建议映射，固件地址改变时修改JSON即可，不必改界面代码。

注意：代码使用从0开始的协议偏移。例如文档中的40001通常在代码中写地址0。

## 测试

```powershell
python -m unittest discover -s tests -p "test_*.py"
python tools\crc_calculator.py "01 03 00 00 00 01"
```

## 安全说明

上位机只负责监控和下发命令。过压、过温、风机故障、水泵故障、膨胀阀限位等保护必须在RL78固件内独立实现。
