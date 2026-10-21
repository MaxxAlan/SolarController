#pragma once

#include <Arduino.h>

// ==============================================================================
// 1. CẤU HÌNH PHẦN CỨNG (PINOUT) - ESP32-S3 N16R8
// ==============================================================================
// LƯU Ý QUAN TRỌNG:
// Module ESP32-S3 N16R8 sử dụng Octal SPI (OPI) cho Flash 16MB và PSRAM 8MB.
// Các chân GPIO 33, 34, 35, 36, 37 được hệ thống sử dụng nội bộ, TUYỆT ĐỐI
// KHÔNG kết nối bất kỳ thiết bị ngoại vi nào vào các chân này để tránh hỏng hóc
// hoặc bootloop.

// 4 Cảm biến quang trở (LDRs) - Kết nối kênh ADC1 (Hoạt động ổn định cả khi bật
// Wi-Fi)
#define PIN_LDR_TOP_LEFT 4     // GPIO 4 (ADC1_CH3)
#define PIN_LDR_TOP_RIGHT 5    // GPIO 5 (ADC1_CH4)
#define PIN_LDR_BOTTOM_LEFT 6  // GPIO 6 (ADC1_CH5)
#define PIN_LDR_BOTTOM_RIGHT 7 // GPIO 7 (ADC1_CH6)

// Động cơ Servo điều khiển góc xoay (Horizontal / Azimuth)
#define PIN_SERVO_HORIZONTAL 41 // GPIO 41 (PWM qua ESP32 LEDC)

// Đo điện áp pin 18650 qua cầu phân áp (Tuỳ chọn)
// Mặc định tắt để tránh báo động giả khi chưa hàn dây phân áp vào ADC
#define ENABLE_BATTERY_SENSE false
#define PIN_BATTERY_ADC 1          // GPIO 1 (ADC1_CH0)
#define VOLTAGE_DIVIDER_RATIO 2.0f // Cầu phân áp 10k:10k (tỉ lệ x2)
#define ADC_VREF 3.3f              // Điện áp tham chiếu ADC (V)
#define ADC_RESOLUTION 4095.0f     // ESP32 12-bit ADC

// Chân còi báo động / LED báo sự cố (Tuỳ chọn)
#define PIN_BUZZER_ALARM 2 // GPIO 2

// ==============================================================================
// 2. THAM SỐ THUẬT TOÁN BÁM SÁNG (SOLAR TRACKING PARAMETERS)
// ==============================================================================
// Ngưỡng Deadband: Chênh lệch ánh sáng tối thiểu giữa 2 bên để kích hoạt xoay
// motor. Giúp tránh rung lắc, giật liên tục và tiết kiệm năng lượng.
#define DEADBAND_THRESHOLD 60

// Ngưỡng ban đêm (Night Mode): Khi ánh sáng trung bình < ngưỡng này => Trời
// tối.
#define NIGHT_THRESHOLD 250

// Giới hạn góc xoay Servo (độ)
#define SERVO_MIN_ANGLE 10     // Giới hạn góc tối thiểu (tránh va chạm cơ khí)
#define SERVO_MAX_ANGLE 170    // Giới hạn góc tối đa
#define SERVO_INITIAL_ANGLE 90 // Vị trí mặc định khi khởi động (chính giữa)
#define SERVO_EAST_PARK_ANGLE                                                  \
  20 // Vị trí hướng Đông (park ban đêm đón sáng sớm)

// Bước dịch chuyển mỗi chu kỳ (độ) và độ trễ để chuyển động mượt mà
#define SERVO_STEP_SIZE 1
#define TRACKING_INTERVAL_MS 80 // Chu kỳ cập nhật góc bám sáng (ms)

// Ngưỡng bảo vệ pin 18650
#define BATTERY_LOW_VOLTAGE 3.3f // Dưới mức này ngắt motor và báo lỗi
#define BATTERY_FULL_VOLTAGE 4.2f

// ==============================================================================
// 3. CẤU HÌNH MẠNG & IOT TELEMETRY (WIFI & FIREBASE)
// ==============================================================================
// Cấu hình Wi-Fi
#define WIFI_SSID "hoangmanh"
#define WIFI_PASSWORD "00000000"
#define WIFI_CONNECT_TIMEOUT_MS                                                \
  12000 // Chờ kết nối Wi-Fi 12s, nếu không được chuyển Offline Failsafe


// Cấu hình Firebase Realtime Database
#define FIREBASE_HOST                                                          \
  "https://"                                                                   \
  "solarcontroller-1cc93-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "XURINmjqSrUgYAs1B7TjQbZoe8Nn151O8BWGRKYs"
#define ENABLE_FIREBASE true // Đã bật kết nối Firebase Cloud

// 1 = đã đấu LDR/servo/mạch pin thật -> gửi số sensor lên Cloud
// 0 = chưa đấu gì (chỉ USB) -> chỉ gửi wifi info, sensor field để null
#define HW_DATA_READY 0

// Chu kỳ gửi dữ liệu Telemetry lên Cloud / Dashboard (ms)
#define TELEMETRY_SEND_INTERVAL_MS 2000

// Kích hoạt Local Web Dashboard tích hợp (Truy cập bằng IP của ESP32 qua trình
// duyệt)
#define ENABLE_LOCAL_WEB_SERVER true
#define LOCAL_WEB_PORT 80
