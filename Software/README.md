# Software — SolarController

Công cụ máy tính: monitor realtime, cấu hình WiFi, điều khiển servo nhanh, và script hỗ trợ Arduino IDE.

## Cấu trúc

```text
Software/
├── Arduino_IDE/                 # Hỗ trợ nạp firmware
│   ├── download_arduino_ide.bat
│   ├── install_esp32_libraries.bat
│   ├── open_controller_sketch.bat
│   └── README.md
├── main.py                      # Menu CLI chính
├── monitor.py                   # [1] Realtime monitor (rich)
├── wifi_setup.py                # [2] Cấu hình WiFi cho ESP32
├── esp_comm.py                  # Serial COM + Firebase RTDB
├── run_servo.py                 # Điều khiển servo nhanh
├── config.json                  # URL Firebase, auth, cổng COM
├── requirements.txt
├── SolarController.bat          # 1-click: tạo .venv + cài thư viện + chạy
└── run.bat                      # Alias của SolarController.bat
```

## Chạy

Nhấp đúp `SolarController.bat` (hoặc `run.bat`). Script sẽ:

1. Tìm Python 3 trên máy.
2. Tạo `.venv` và cài `requirements.txt` nếu chưa có.
3. Mở menu:

- `[1]` Realtime Monitor — 4 LDR, pin, trạng thái servo, phím tắt điều khiển.
- `[2]` Setting WiFi — gửi SSID/password xuống ESP32 qua Serial hoặc Firebase, lưu NVS.
- `[3]` Exit.

## Phím tắt trong monitor

| Phím | Chức năng |
|---|---|
| `Q` / `Esc` | Thoát |
| `A` / `M` | AUTO / MANUAL |
| `+` / `-` | Nhịp quay CW / CCW 200 ms |
| `0` `9` `6` `8` | Bắc / Đông / Nam / Tây |
| `T` | Quét test 4 hướng |
| `P` | In trạng thái từ firmware |

> Servo continuous không có feedback góc: lệnh "góc" được quy về hướng gần nhất, còn hiển thị là hướng đích + thời gian nhịp chạy gần nhất (`last_target`, `last_ms`).

## Ghi chú

- `config.json` chứa URL Firebase và legacy token của project `solarcontroller-1cc93`.
- Cùng một lệnh được gửi song song qua Serial và Firebase; kênh nào tới trước thì áp dụng trước.
- Firmware bỏ qua lệnh có `timestamp` cũ hơn lệnh đã áp dụng gần nhất.
