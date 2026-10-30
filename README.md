# SolarController ☀️🤖

Hệ thống điều khiển & giám sát pin năng lượng mặt trời tự động bám sáng (single-axis solar tracker), dùng ESP32-S3 + servo continuous + 4 LDR, telemetry qua Firebase RTDB và dashboard web.

---

## 📌 Tính năng chính

- **Bám sáng tự động (AUTO)**: so sánh 4 LDR (TL/TR/BL/BR), quay servo theo nhịp `TRACK_PULSE_MS` cho tới khi |diff| < deadband.
- **Ban đêm**: tổng sáng thấp hơn ngưỡng → park về hướng Đông (TL) → light-sleep tiết kiệm pin.
- **Thủ công (MANUAL)** từ dashboard: chạy 4 hướng TL/TR/BL/BR, về home, dừng khẩn cấp, chỉnh tay CW/CCW.
- **Hiệu chỉnh thời gian chạy** (servo continuous không có feedback góc): đo bằng web — bấm **Đo**, servo quay liên tục, vừa đúng vị trí bấm **GHI**; ESP trả `last_ms` + `move_seq`, web lưu rồi tự về home.
- **Lưu cấu hình vào NVS**: `home_ms`, `tl/tr/bl/br_ms`, `*_dir`, `cw/ccw/stop_us` sống qua reset.
- **Nguồn**: TP4056 sạc 18650 từ panel, MT3608 boost; đo VBAT qua divider, tính SOC, cảnh báo pin yếu.
- **CLI Python**: monitor realtime + cấu hình WiFi cho ESP qua Serial/Firebase.

---

## 🛠️ Cấu hình phần cứng

| Thành phần | Chi tiết |
|---|---|
| Vi điều khiển | ESP32-S3 DevKit N16R8 (Arduino IDE) |
| LDR | 4x module quang trở LM393, AO → `GPIO4, 5, 6, 7` (ADC1); DO → `GPIO15, 16, 17` |
| Servo | SG90 **cắt trục** (continuous) — signal `GPIO18`, VCC 5V nguồn ngoài, GND chung |
| Đo pin | Divider 100k/100k BAT+ → `GPIO8` |
| Nguồn | Panel → TP4056 → 18650 → MT3608 → 5V/3V3 |

> Servo continuous: `500us` = CCW hết tốc, `1500us` = dừng, `2400us` = CW hết tốc. Vị trí suy ra từ **thời gian chạy**, không đọc được góc thật.

---

## 📂 Cấu trúc thư mục

```text
SolarController/
├── Code Base/
│   ├── arduino-controller/
│   │   ├── controller/controller.ino   # Firmware chính (motion-v2)
│   │   ├── ldr_test/ldr_test.ino       # Test 4 LDR + ADC
│   │   └── servo_test/servo_test.ino   # Test servo qua Serial
│   └── web-page/Dashboard/             # Dashboard (Firebase Hosting public)
│       ├── index.html
│       ├── app.js
│       └── style.css
├── Software/
│   ├── Arduino_IDE/                    # Script tải IDE / thư viện / mở sketch
│   ├── main.py                         # Menu CLI
│   ├── monitor.py                      # Realtime monitor (rich)
│   ├── wifi_setup.py                   # Cấu hình WiFi ESP32
│   ├── esp_comm.py                     # Serial + Firebase RTDB
│   ├── run_servo.py                    # Điều khiển servo nhanh
│   ├── config.json                     # URL Firebase + COM
│   ├── requirements.txt
│   ├── SolarController.bat             # 1-click: tạo .venv + chạy CLI
│   └── run.bat
├── Diagram/                            # Sơ đồ nguyên lý, block, flowchart (drawio + png)
├── Report/                             # Báo cáo DOCX/PDF
├── firebase.json / database.rules.json / .firebaserc
└── README.md
```

---

## ⚡ Chạy nhanh

### 1. Nạp firmware (Arduino IDE)

1. Mở `Code Base/arduino-controller/controller/controller.ino` (hoặc chạy `Software/Arduino_IDE/open_controller_sketch.bat`).
2. Cài board **ESP32-S3 DevKit** và 3 thư viện: `ESP32Servo`, `ArduinoJson`, `Firebase ESP Client` (script `install_esp32_libraries.bat` hỗ trợ).
3. Sửa `WIFI_SSID` / `WIFI_PASSWORD` / `FIREBASE_AUTH` ở đầu file nếu cần.
4. Chọn board **ESP32S3 Dev Module**, Flash 16MB, chọn đúng cổng COM → Upload.
5. Serial Monitor 115200: `h` = help, `p` = trạng thái, `tl/tr/bl/br` = chạy tới hướng, `run TL` = quay liên tục để đo, `s` = dừng.

### 2. Dashboard web

Mở trực tiếp `Code Base/web-page/Dashboard/index.html`, hoặc deploy:

```bash
firebase deploy --only hosting,database --project solarcontroller-1cc93
```

Dashboard đọc `https://.../solar_controller/telemetry` (public read) và ghi lệnh vào `/solar_controller/command` (public write). Firmware dùng legacy token, xoá node command sau khi áp dụng và bỏ qua lệnh có `timestamp` cũ.

### 3. CLI Python

```bat
Software\SolarController.bat
```

Lần đầu tự tạo `.venv` và cài `pyserial`, `requests`, `rich`, `colorama`. Menu: `[1]` monitor realtime, `[2]` cấu hình WiFi, `[3]` thoát.

---

## 🔄 Luồng lệnh & telemetry

```text
Dashboard --(set /solar_controller/command)--> ESP32
  { mode, move:"TL|TR|BL|BR|HOME", run:"...", stop:true,
    dir:"CW|CCW", ms, home_ms, tl_ms, ..., timestamp }
ESP32 --(set /solar_controller/telemetry)--> Dashboard
  { mode, moving, move_target, move_seq, last_target, last_ms,
    ldr_*, diff, vbat, soc, charge, fault, wifi_*, fw:"motion-v2", ... }
```

- Telemetry chỉ ghi khi `mvPhase == MV_IDLE` để không chặn vòng lặp khi servo đang chạy.
- Khi đang chạy, firmware poll `/command` mỗi 150ms để nhận lệnh DỪNG nhanh.

---

## 🔧 Hiệu chỉnh servo (quan trọng)

1. Về home: bấm **Về home** → servo quay CCW; chạm mốc TL thì bấm **■ GHI** → lưu `home_ms`.
2. Đo từng hướng: bấm **▶ Đo** ở TL/TR/BL/BR → servo quay liên tục; đúng hướng bấm **■ GHI** → lưu `*_ms`, web tự về home.
3. Bấm **Chạy** để kiểm tra: home → chạy đúng số ms đã lưu.

Thời gian chạy phụ thuộc pin/servo, nên hiệu chỉnh lại khi thay servo hoặc nguồn.
