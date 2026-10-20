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

- **Vi điều khiển**: ESP32-S3-WROOM-1 (Hỗ trợ 2.4GHz Wi-Fi & Bluetooth 5 LE)
- **Cảm biến quang học**: 4x Module quang trở (LDR Sensor Module with LM393 comparator)
- **Động cơ**: Servo Motor (PWM Control)
- **Hệ thống nguồn**:
  - Tấm pin năng lượng mặt trời (Solar Panel)
  - Mạch sạc & bảo vệ pin TP4056 (Type-C)
  - Pin Li-ion 18650
  - Mạch tăng áp DC-DC Boost Converter MT3608

---

## 📂 Cấu trúc thư mục

```text
SolarController/
├── Code Base/              # Mã nguồn chương trình cho ESP32-S3
├── Cricular Diagram/       # Sơ đồ kết nối phần cứng và mạch nguyên lý
├── Flow Chart/             # Lưu đồ thuật toán vận hành hệ thống (Draw.io & ảnh PNG)
├── .gitignore              # Bỏ qua các file cài đặt và file tạm
└── README.md               # Tài liệu giới thiệu dự án
```