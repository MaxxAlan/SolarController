# Thư Mục Software - SolarController ☀️💻

Thư mục chứa toàn bộ phần mềm ứng dụng máy tính và công cụ lập trình nạp code cho hệ thống SolarController. Được thiết kế theo chuẩn di động (portable) để **chỉ cần clone từ Git về bất kỳ máy tính nào là có thể chạy ngay lập tức**.

---

## 📂 Cấu trúc thư mục

```text
Software/
├── Arduino_IDE/                   # 1. Công cụ & Hướng dẫn cho Arduino IDE (Nạp Firmware)
│   ├── download_arduino_ide.bat   # Mở trang tải Arduino IDE 2.x chính thức
│   ├── install_esp32_libraries.bat# Tự động tải 3 thư viện (ESP32Servo, ArduinoJson, Firebase)
│   ├── open_controller_sketch.bat # 1-click mở file firmware trong Code Base
│   └── README.md                  # Hướng dẫn chi tiết thiết lập board ESP32-S3
│
├── Python/                        # 2. Bộ phần mềm giám sát & giao tiếp Python CLI
│   ├── main.py                    # Chương trình điều hướng chính (Menu CLI)
│   ├── monitor.py                 # Menu 1: Realtime Monitor (4 LDR, Pin, Voltage, Hướng Servo)
│   ├── wifi_setup.py              # Menu 2: Cài đặt WiFi cho ESP32 lưu Flash NVS
│   ├── esp_comm.py                # Giao tiếp kép: Serial COM (auto) + Firebase RTDB
│   ├── run_servo.py               # Script runner & API điều khiển Servo
│   ├── config.json                # Cấu hình cổng COM và Firebase Cloud
│   ├── requirements.txt           # Thư viện yêu cầu (pyserial, requests, rich)
│   └── setup_env.bat              # Script tự tạo venv và cài thư viện độc lập
│
├── SolarController.bat            # File thực thi chính (.bat) khởi chạy menu 3 lựa chọn
└── run.bat                        # Lối tắt khởi động nhanh phần mềm
```

---

## 🚀 Cách chạy chương trình

Chỉ cần **nhấp đúp chuột** vào:
👉 [`SolarController.bat`](../SolarController.bat) (ở thư mục gốc hoặc trong thư mục `Software/`)

File `.bat` sẽ tự động:
1. Nhận diện Python trên máy tính.
2. Tự động khởi tạo môi trường ảo `.venv` và tự cài đặt các thư viện cần thiết (`pyserial`, `requests`, `rich`).
3. Tự động dò tìm cổng Serial COM của ESP32 (hoặc chuyển sang kết nối Cloud).
4. Mở Menu 3 lựa chọn:
   - `[1]` Hiển thị trạng thái tổng thể (Realtime CLI Monitor)
   - `[2]` Cài đặt WiFi cho ESP32 (Setting WiFi)
   - `[3]` Thoát chương trình (Exit)
