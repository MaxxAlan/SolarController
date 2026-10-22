# Code Base - Firmware ESP32-S3 ⚡☀️

Thư mục chứa toàn bộ mã nguồn nhúng (C++/Arduino) chạy trên vi điều khiển **ESP32-S3 DevKitC-1 (N16R8)**.

---

## 📂 Các Firmware trong `Code Base/`

| Thư mục | Tệp tin | Mục đích |
|---|---|---|
| `controller/` | [`controller.ino`](controller/controller.ino) | **Firmware chính thức**: Tự động bám sáng (Dual-Axis/Single-Axis), đo 4 LDR, đo điện áp pin 18650, điều khiển Servo PWM, gửi Telemetry lên Firebase và giao tiếp Serial hai chiều. |
| `demo/` | [`demo.ino`](demo/demo.ino) | Firmware chạy thử nghiệm / trình diễn các chế độ quay quét của động cơ Servo và kiểm tra cảm biến. |
| `ldr_test/` | [`ldr_test.ino`](ldr_test/ldr_test.ino) | Firmware kiểm tra độc lập 4 module quang trở LM393 (AO + DO), cân chuẩn (Calibration min/max) trước khi lắp ráp vào khung cơ khí. |

---

## 🔌 Sơ đồ đấu chân phần cứng (Pinout)

```text
========================================================================================
     CẢM BIẾN / THIẾT BỊ       CHÂN TRÊN ESP32-S3          GHI CHÚ
========================================================================================
 1.  LDR TL (Đông / Trên-Trái)     GPIO 4 (ADC1_CH3)      Ngõ ra Analog AO
 2.  LDR TR (Tây / Trên-Phải)      GPIO 5 (ADC1_CH4)      Ngõ ra Analog AO
 3.  LDR BL (Nam / Dưới-Trái)     GPIO 6 (ADC1_CH5)      Ngõ ra Analog AO
 4.  LDR BR (Bắc / Dưới-Phải)     GPIO 7 (ADC1_CH6)      Ngõ ra Analog AO
 5.  LDR DO TL / TR / BL           GPIO 15, 16, 17        Ngõ ra so sánh số Digital
 6.  Động cơ Servo                 GPIO 18                Tín hiệu PWM (50Hz), nguồn ngoài 5V
 7.  Đo điện áp pin (VBAT)         GPIO 8 (ADC1_CH7)      Cầu phân áp 100k/100k (tỉ lệ 2.0x)
 8.  Còi báo sự cố (Buzzer)       GPIO 11 (Tùy chọn)     Báo động pin yếu / lỗi cảm biến
========================================================================================
```
