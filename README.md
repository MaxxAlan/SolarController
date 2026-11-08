# ☀️ Remote Solar Panel Management

Uno điều khiển và tracking, ESP32-S3 làm cổng WiFi và cloud. LCD 2004 và web cùng hiển thị trạng thái, V, I, P, mức pin.

**Vai trò của relay:** relay đóng/ngắt đường điện từ tấm pin vào TP4056 (panel → INA219 → relay COM–NC → TP4056). Đóng thì pin được sạc, ngắt thì dừng sạc. Khi ngắt, Uno, ESP32, LCD và servo vẫn chạy bằng pin qua MT3608 nên vẫn báo trạng thái và nhận lệnh đóng lại.

**Về web + internet:** ESP32-S3 **không tự host trang lên internet** — nó chỉ là client WiFi, đo/đẩy dữ liệu lên Firebase RTDB và nhận lệnh `/cmd`. Trang public do **Firebase Hosting** phục vụ: `https://solarcontroller-1cc93.web.app/`. Do đó "ESP không lên internet" luôn nghĩa là "ESP chưa nối được WiFi" — xử lý ở mục 3.

[Sơ đồ khối](#1-sơ-đồ-khối) · [Lưu đồ](#2-lưu-đồ-thuật-toán-uno) · [WiFi lần đầu](#3-cấu-hình-wifi-lần-đầu-esp) · [Tài liệu](#4-tài-liệu)

## 1. Sơ đồ khối

```mermaid
flowchart LR
  SP["Solar panel 5-6V"] --> INA["INA219 đo V và I"]
  INA -- đường điện từ panel --> RLY["Relay đóng/ngắt đường sạc"]
  RLY -- "đóng: cấp điện sạc / ngắt: dừng sạc" --> TP["TP4056 sạc pin"]
  TP --> BAT["Pin 18650 3.7V"]
  BAT --> MT["MT3608 boost 5V (chạy từ pin, vẫn hoạt động khi relay ngắt)"]
  MT -. 5V .-> UNO["Arduino Uno"]
  MT -. 5V .-> ESP["ESP32-S3"]
  MT -. 5V .-> SRV["2 servo SG90"]
  MT -. 5V .-> LCD["LCD 2004 I2C"]
  INA -- I2C --> UNO
  LDR["4 LDR A0-A3"] --> UNO
  UNO -- D7 kích relay --> RLY
  UNO -- D9 D10 --> SRV
  UNO -- I2C --> LCD
  UNO <-- "UART D0/D1 + chia áp về 3.3V (10k/20k)" --> ESP
  BAT -- chia 100k/100k --> ESP
  ESP -- WiFi (client 2.4GHz) --> FB[("Firebase RTDB")]
  FB <--> WEB["Firebase Hosting: solarcontroller-1cc93.web.app"]
```

Lưu ý đi dây UART: TX Uno 5V → RX ESP 3.3V phải qua chia áp về ~3.3V
(10k/20k như comment trong `solar_gateway.ino`, hoặc 1k/2k — cùng tỉ lệ ~1/3).
Mạch pin ESP: chia 100k/100k vào GPIO4.

## 2. Lưu đồ thuật toán (Uno)

Khớp với `uno_solar_tracker.ino` (không có nút bấm vật lý — mọi lệnh điều khiển
đến từ web qua ESP):

```mermaid
flowchart TD
  S(["Bật nguồn"]) --> I["Khởi tạo LCD, INA219, servo, relay OFF, UART 9600"]
  I --> L["Chu kỳ 50 ms: đọc 4 LDR"]
  L --> E{"Nhận lệnh ESTOP từ web (R,3)?"}
  E -- Có --> ES["Relay NGẮT và KHÓA, LCD báo NGAT KHAN CAP (chỉ web Recover mới bỏ)"]
  ES --> R{"Web gửi Recover (emergency=false)?"}
  R -- Chưa --> ES
  R -- Rồi --> E
  E -- Không --> M{"Chế độ MANUAL?"}
  M -- Có --> MA["Servo bước 1° về panTarget/tiltTarget từ web"]
  M -- Không --> N{"Độ sáng TB < 60 (trời tối)?"}
  N -- Có --> W["Dừng bám; tối liên tục >= 60 s -> servo về HOME (0/0)"]
  N -- Không --> T["Bám sáng: sai lệch LDR > 20 -> bước 1°/50 ms về phía sáng hơn"]
  T --> G{"Relay auto: ap panel >= 3.7V (hysteresis ngắt < 3.3V giữ 5 s) và pin chưa đầy?"}
  G -- Đủ điều kiện --> RD["Đóng relay sạc; pin >= STOP(100%) thì ngắt, sac lai <= RESUME(90%)"]
  G -- Không --> LOOP
  MA --> LOOP
  W --> LOOP
  RD --> LOOP
  LOOP["Mỗi 250 ms: đo INA219 + cộng Wh; mỗi 1 s: cập nhật LCD + gói U sang ESP"] --> L
```

Ghi chú:
- Mất link ESP > 4 s, UNO tự về relay auto an toàn; E-stop chỉ đến từ web.
- Ngưỡng relay (3.7V/3.3V, STOP/RESUME) và giới hạn góc do admin đổi trên
  dashboard → ESP gửi sang UNO bằng gói `K`.

## 3. Cấu hình WiFi lần đầu (ESP)

ESP chưa từng nối WiFi (hoặc sai mật khẩu) sẽ tự mở Access Point riêng:

1. Cấp nguồn ESP (5 V, ≥ 500 mA), mở **Serial Monitor 115200** — thấy dòng
   `[WIFI] Chua co WiFi dang luu -> mo portal config` hoặc
   `[WIFI] Ket loi that bai -> mo portal`.
2. Điện thoại/PC vào WiFi **`SolarTracker-Setup`**, mật khẩu **`12345678`**.
3. Trang **Cau hinh WiFi** tự bật. Nếu không bật, mở tay **http://192.168.4.1/setup**.
4. Nhập SSID (vd `hoangmanh`) + mật khẩu WiFi → **LUU VA KHOI DONG LAI**.
   ESP lưu vào flash rồi tự khởi động lại.
5. Serial thấy `[WIFI] OK, IP=...` là nối được; dashboard đổi trạng thái `ONLINE`.

**Lưu lại vào portal mà vẫn quay lại portal:** mật khẩu sai, hoặc router không hợp.
Nguyên nhân thường gặp:

| Nguyên nhân | Cách sửa |
|---|---|
| Router/SSID chỉ 5 GHz | ESP32-S3 **chỉ hỗ trợ 2.4 GHz** — bật 2.4 GHz hoặc dùng SSID 2.4 GHz |
| Sai mật khẩu WiFi | nhập lại ở portal (sai mật khẩu sẽ tự mở portal lại sau 30 s) |
| SSID bị ẩn | bật phát SSID |
| Router chặn MAC / AP isolation | cho phép thiết bị |
| Nguồn yếu (USB máy tính, MT3608 bị tải) | dùng nguồn 5 V ổn định ≥ 500 mA |

Portal tự khởi động lại sau 10 phút không thao tác; mất WiFi liên tục 1 phút thì portal tự mở lại.
Khi portal đang chạy, UART Uno–ESP vẫn chạy nên LCD vẫn hiển thị % pin.

**Đổi WiFi về sau:** trên dashboard mục "Doi WiFi ESP32" (ghi `/cmd/wifiSsid`,
`/cmd/wifiPass`, `/cmd/wifiTick`, ESP tự lưu + reboot) — chỉ dùng khi ESP còn
online; nếu ESP đã mất WiFi thì vào lại `SolarTracker-Setup` như trên.

## 4. Tài liệu

- [Mã nguồn + README chi tiết (UART protocol, Firebase, deploy, troubleshooting)](SE2044_G1_IOT102t_FinalReport/SourceCode/README.md)
- Report / Slides / Video demo: `SE2044_G1_IOT102t_FinalReport/Document`, `Video Demo`
- Bài nộp LMS: `LMS/`
