# ESP32 BLE Joystick (PlatformIO + Arduino)

这个项目把 **ESP32 DevKit V1** 做成一个 BLE 摇杆发送端，面向 **Debian** 主机接收。

输入包括：

- 2 路摇杆模拟量：`X`、`Y`
- 1 路摇杆按压开关：`SW`
- 2 路独立按钮：`BTN1`、`BTN2`

ESP32 通过 **NimBLE-Arduino** 创建一个自定义 BLE GATT 服务，并周期性通过 **Notify** 发送固定 **5 字节** 数据包。

---

## 1. 项目结构

```text
esp32_ble_joystick/
├─ platformio.ini
├─ include/
│  ├─ config.h
│  ├─ calibration_store.h
│  ├─ input_manager.h
│  └─ ble_server.h
├─ src/
│  ├─ main.cpp
│  ├─ calibration_store.cpp
│  ├─ input_manager.cpp
│  └─ ble_server.cpp
└─ scripts/
   └─ recv_ble.py
```

模块说明：

- `config.h`：引脚、UUID、滤波和校准参数
- `calibration_store.*`：校准数据持久化到 NVS/Preferences
- `input_manager.*`：ADC 采样、滤波、中心/外圈校准、按键扫描
- `ble_server.*`：BLE 初始化、服务创建、广播、通知发送
- `main.cpp`：主流程编排
- `scripts/recv_ble.py`：Debian 接收端程序

---

## 2. 硬件连接

推荐引脚：

- `GPIO34` -> 摇杆 `VRx`
- `GPIO35` -> 摇杆 `VRy`
- `GPIO25` -> 摇杆按压 `SW`
- `GPIO26` -> `BTN1`
- `GPIO27` -> `BTN2`

按钮接法：

- 一端接 GPIO
- 一端接 GND
- 程序内部使用 `INPUT_PULLUP`

注意：

- `GPIO34`、`GPIO35` 是输入专用，适合 ADC 采样
- 摇杆模拟量请确保不超过 `3.3V`

---

## 3. BLE 连接协议

### 设备名

```text
ESP32_Gamepad
```

### Service UUID

```text
12345678-1234-1234-1234-1234567890ab
```

### Characteristic UUID

```text
abcd1234-5678-1234-5678-1234567890ab
```

### 通知数据格式

每次发送固定 **5 字节**：

```text
byte0~1 : X 轴，uint16，小端
byte2~3 : Y 轴，uint16，小端
byte4   : 按键位图，uint8
```

对应 C 结构体：

```c
struct Packet {
    uint16_t joy_x;
    uint16_t joy_y;
    uint8_t buttons;
};
```

### 按键位定义

- `bit0` -> `SW`
- `bit1` -> `BTN1`
- `bit2` -> `BTN2`

例子：

- `0x00`：都没按
- `0x01`：只按下 `SW`
- `0x02`：只按下 `BTN1`
- `0x04`：只按下 `BTN2`
- `0x07`：三个都按下

---

## 4. PlatformIO 配置

`platformio.ini`：

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =
    h2zero/NimBLE-Arduino@^2.1.0

build_flags =
    -D CORE_DEBUG_LEVEL=0
```

编译：

```bash
pio run
```

烧录：

```bash
pio run -t upload
```

串口监视：

```bash
pio device monitor
```

---

## 5. 校准机制

### 首次启动

程序会先尝试从 NVS 读取校准数据：

- 如果读到有效数据，直接使用
- 如果没有有效数据，自动执行完整校准并保存

### 完整校准流程

#### 中心校准

上电后先不要碰摇杆，程序会测量：

- `centerX`
- `centerY`

#### 外圈校准

随后程序会给几秒时间让你把摇杆推到四周画圈，记录：

- `minX`
- `maxX`
- `minY`
- `maxY`

之后把原始 ADC 值映射为统一输出：

- 左/下 -> 接近 `0`
- 中心 -> 接近 `2048`
- 右/上 -> 接近 `4095`

### 运行中重校准

同时按住 **BTN1 + BTN2 2 秒**：

- 重新执行完整校准
- 自动保存到 NVS

---

## 6. 串口输出说明

程序运行时会打印：

- 原始 ADC：`RAW_X`、`RAW_Y`
- 输出值：`OUT_X`、`OUT_Y`
- 按键状态：`SW`、`BTN1`、`BTN2`
- 蓝牙连接状态：`BLE`

示例：

```text
==== ESP32 BLE Joystick ====
[CAL] Loaded calibration from NVS.
[CAL] Result: minX=510 centerX=1850 maxX=3530 | minY=460 centerY=1824 maxY=3470
[BLE] Local MAC: aa:bb:cc:dd:ee:ff
[BLE] Advertising started
[SYS] Waiting for BLE client...
RAW_X=1848 RAW_Y=1825 OUT_X=2048 OUT_Y=2048 SW=0 BTN1=0 BTN2=0 BLE=1
```

---

## 7. Debian 连接过程

### 安装依赖

```bash
sudo apt update
sudo apt install -y bluetooth bluez python3-pip
pip3 install bleak
```

### 启动蓝牙服务

```bash
sudo systemctl enable bluetooth
sudo systemctl start bluetooth
```

### 可选：用 bluetoothctl 扫描设备

```bash
bluetoothctl
```

进入交互后：

```text
power on
agent on
default-agent
scan on
```

正常时可以看到：

```text
[NEW] Device AA:BB:CC:DD:EE:FF ESP32_Gamepad
```

---

## 8. Debian 接收程序

项目自带：

```text
scripts/recv_ble.py
```

运行：

```bash
python3 scripts/recv_ble.py
```

程序逻辑：

- 扫描名为 `ESP32_Gamepad` 的 BLE 设备
- 连接到目标地址
- 订阅特征 UUID 的通知
- 解析 5 字节数据包
- 打印 `X`、`Y`、`SW`、`BTN1`、`BTN2`

示例输出：

```text
scanning...
connecting to: AA:BB:CC:DD:EE:FF
connected: True
receiving... Ctrl+C to stop
X=2048 Y=2048 SW=0 BTN1=0 BTN2=0
X=4095 Y=2048 SW=0 BTN1=1 BTN2=0
```

---

## 9. 常见问题

### 扫描页只显示 MAC 或 `N/A`

有些 BLE 工具扫描页不直接显示设备名。可以：

- 对照串口打印出来的本机 MAC
- 连接后查看 Service UUID 是否是项目里的 UUID

### 静止时 ADC 有抖动

代码里已经做了：

- 多次平均采样
- 低通滤波
- 中心死区
- 校准后的分段映射

### 为什么用 NimBLE-Arduino

因为它更适合 ESP32 上这种自定义 BLE 外设场景，资源占用更低，也更稳定。

---

## 10. 后续可继续升级

下一步最值得加的是：

- Debian 侧把 BLE 数据映射为虚拟手柄设备
- 在 ESP32 侧增加电量上报
- 增加按键去抖
- 保存更多用户配置参数

