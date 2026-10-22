# Hướng Dẫn Nạp Firmware ESP32-S3 với Arduino IDE ⚡

Tài liệu hướng dẫn cấu hình và nạp code từ thư mục `Code Base/` lên vi điều khiển **ESP32-S3 DevKitC-1 N16R8** trên bất kỳ máy tính nào.

---

## 🛠️ 1. Cài đặt môi trường Arduino IDE

1. **Tải Arduino IDE 2.x**:
   - Chạy tệp [`download_arduino_ide.bat`](download_arduino_ide.bat) để mở trang tải chính thức: [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software).
2. **Thêm Package ESP32**:
   - Mở Arduino IDE -> Vào `File` -> `Preferences` (hoặc `Ctrl + ,`).
   - Tại mục **Additional boards manager URLs**, dán đường link sau:
     ```text
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Vào `Tools` -> `Board` -> `Boards Manager...` -> Tìm `esp32` của **Espressif Systems** và bấm **Install**.

---

## 📚 2. Cài đặt các thư viện bắt buộc

Chạy tệp [`install_esp32_libraries.bat`](install_esp32_libraries.bat) để tự động tải 3 thư viện hoặc mở `Sketch` -> `Include Library` -> `Manage Libraries...` (`Ctrl + Shift + I`) tìm và cài:

1. **`ESP32Servo`** (bởi Kevin Harrington / John K. Bennett)
2. **`ArduinoJson`** (phiên bản **7.x**, bởi Benoit Blanchon)
3. **`Firebase Arduino Client Library for ESP8266 and ESP32`** (bởi Mobizt)

---

## ⚙️ 3. Cấu hình thông số Board trong Arduino IDE (`Tools`)

| Thiết lập | Giá trị chọn |
|---|---|
| **Board** | `ESP32S3 Dev Module` |
| **Flash Size** | `16MB (128Mb)` |
| **Partition Scheme** | `16MB (3MB APP/9.9MB FATFS)` hoặc `Default 4MB with spiffs` |
| **PSRAM** | `OPI PSRAM` |
| **USB CDC On Boot** | `Disabled` (nếu cắm cổng UART/CH343) / `Enabled` (nếu cắm Native USB) |
| **CPU Frequency** | `240MHz (WiFi)` |
| **Upload Speed** | `921600` hoặc `115200` |
| **Port** | Chọn cổng COM của ESP32 (ví dụ `COM5`) |

---

## 🚀 4. Nạp Firmware

Chạy tệp [`open_controller_sketch.bat`](open_controller_sketch.bat) để tự động mở file firmware chính [`Code Base/controller/controller.ino`](../../Code%20Base/controller/controller.ino). Bấm nút **Upload (`➡`)** để nạp code vào board.
