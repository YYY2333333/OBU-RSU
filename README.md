# 可变标志系统（基于 ESP8266 与 ESP32）

本项目为基于 ESP8266 和 ESP32 的智能交通可变标志系统，旨在实现车辆识别、交互式信息显示与远程管理功能。项目采用 MAC 嗅探与 Web 控制技术。

### 车辆端（ESP32）

- 📡 **MAC 嗅探**：监听周围设备的 MAC 地址，用于判断路侧接近。
- 🪧 **标志显示**：根据识别结果在显示屏上动态更新标志信息。
- 📈 **接收数量统计**：统计识别到的唯一设备数量。


### 路侧端（ESP8266）

- 🌐 **Web 页面配置**
  - 修改允许通信的 MAC 地址列表。
  - 设置 ESP8266 的发射功率。
  - *自定义 MAC（新增）：支持自定义设置虚拟 MAC 地址（存在检查功能），用于调试或模拟。
- 💡 **信息显示**：展示当前系统状态、接收情况等信息。
- 🔄 **OTA 在线升级**：支持通过网页进行固件更新，无需物理接线。

## 项目特色

- 🌍 基于 Wi-Fi 通信，无需额外网络模块。
- 📶 动态可调发射功率，适配不同部署场景。
- 🔧 所有配置均可通过 Web 界面操作，简洁直观。
- 🔁 支持 OTA，便于远程维护与版本迭代。
- 🧪 自定义 MAC 支持，有助于测试与模拟真实交通场景。

## 使用说明

### 硬件要求

- ESP32 开发板 ×1（车辆端）
- ESP8266（如 NodeMCU）×1（路侧端）
- OLED 或 LCD 显示屏（可选）

### 软件要求

- Arduino IDE（或 PlatformIO）


## 文件结构
- ESP32内为源码与屏幕定义的引脚的头文件。该文件（User_Setup.h）需要放在屏幕的库文件内。
- ESP8266内为源码和编译后的文件
- JPG内为ESP32显示图片
### 日志
- 2025年6月9日
  - 修复：MAC自定义WEB误报错（ESP8266）
  - 新增：MAC地址简易输入（ESP8266）
