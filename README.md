# SolarController ☀️🤖

Hệ thống điều khiển và giám sát pin năng lượng mặt trời tự động bám sáng (Dual/Single-Axis Solar Tracker) tích hợp IoT Telemetry và Dashboard.

---

## 📌 Tính năng chính

- **Tự động bám sáng (Auto Tracking)**: Đo lường cường độ ánh sáng qua 4 cảm biến quang trở (LDR) và điều khiển góc xoay của động cơ Servo bám theo góc chiếu sáng tối ưu.
- **Chế độ ban đêm (Night Mode)**: Tự động phát hiện khi trời tối, trả tấm pin về vị trí ban đầu (hướng Đông) và chuyển sang chế độ tiết kiệm năng lượng (Low-power standby).
- **Chế độ thủ công (Manual Mode)**: Cho phép điều khiển góc xoay của tấm pin trực tiếp qua Dashboard (thanh trượt / nút bấm).
- **Quản lý & Bảo vệ nguồn (Power Management)**:
  - Tự sạc pin 18650 từ tấm pin mặt trời thông qua module sạc bảo vệ TP4056.
  - Chống sạc quá mức (Overcharge protection) và tự động duy trì sạc thả nổi (Float charge).
  - Tự động ngắt động cơ và cảnh báo khi pin chạm ngưỡng cạn kiệt.
- **Giám sát thời gian thực (IoT Dashboard)**:
  - Gửi dữ liệu telemetry (Điện áp, Dòng sạc, SOC, Cường độ LDR, Góc servo) qua Wi-Fi / BLE.
  - Nhận lệnh điều khiển thời gian thực và phát cảnh báo sự cố.

---

## 🛠️ Cấu hình phần cứng

- **Vi điều khiển**: ESP32-S3-N16R8
  - Hỗ trợ 2.4GHz Wi-Fi (802.11 b/g/n) & Bluetooth 5 LE.
- **Cảm biến quang học**: 4x Module quang trở (LDR Sensor Module with LM393 comparator) kết nối kênh ADC1 (`GPIO 4, 5, 6, 7`).
- **Động cơ**: Servo Motor (PWM Control trên `GPIO 41`).
- **Hệ thống nguồn**:
  - Tấm pin năng lượng mặt trời (Solar Panel)
  - Mạch sạc & bảo vệ pin TP4056 (Type-C)
  - Pin Li-ion 18650 mAh
  - Mạch tăng áp DC-DC Boost Converter MT3608

---

## 📂 Cấu trúc thư mục

```text
SolarController/
├── include/                # Header cấu hình (chân, ngưỡng bám sáng, WiFi, Firebase)
│   └── config.h
├── src/                    # Mã nguồn C++ cho ESP32-S3 (Tracker, Battery, Network, Main)
│   ├── main.cpp
│   ├── tracker.cpp / .h
│   ├── battery_monitor.cpp / .h
│   └── network_manager.cpp / .h
├── Dashboard/              # Web Dashboard giám sát & mô phỏng 3D thời gian thực (Three.js)
│   ├── index.html
│   ├── style.css
│   └── app.js
├── Cricular Diagram/       # Sơ đồ kết nối phần cứng và mạch nguyên lý
├── Flow Chart/             # Lưu đồ thuật toán vận hành hệ thống (Draw.io & ảnh PNG)
├── platformio.ini          # Cấu hình PlatformIO chuẩn cho ESP32-S3-DevKitC-1 N16R8
├── .gitignore              # Bỏ qua các file build và file tạm
└── README.md               # Tài liệu giới thiệu dự án
```

---

## ⚡ Hướng dẫn nạp Firmware nhanh với PlatformIO

1. Mở thư mục dự án `SolarController` bằng **VS Code** (đã cài extension **PlatformIO IDE**).
2. Tùy chỉnh thông tin Wi-Fi và ngưỡng cảm biến tại [`include/config.h`](include/config.h).
3. Kết nối board ESP32-S3 vào máy tính qua cổng USB (CH343 UART hoặc Native USB).
4. Bấm nút **Build** (`✔`) và **Upload** (`➡`) trên thanh công cụ PlatformIO ở đáy màn hình VS Code.
5. Mở **Serial Monitor** (115200 baud) để theo dõi thông số hoặc mở [`Dashboard/index.html`](Dashboard/index.html) để theo dõi mô phỏng 3D & telemetry thời gian thực.

