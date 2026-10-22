"""
SolarController - Python Runner & Servo Controller (run_servo.py)
Script tiện ích đa năng có thể:
  1. Khởi chạy trực tiếp file .bat (SolarController.bat) từ Python.
  2. Khởi chạy giao diện CLI Menu hoặc Monitor điều khiển Servo.
  3. Cung cấp API Python điều khiển góc Servo, test quét Servo, đọc dữ liệu 4 LDR, Pin, Điện áp.

Cách dùng từ dòng lệnh:
  python run_servo.py          -> Khởi chạy chương trình (hoặc gọi file .bat)
  python run_servo.py --bat    -> Bật file SolarController.bat trong cửa sổ mới
  python run_servo.py --sweep  -> Quét thử nghiệm Servo (0° -> 180° -> 90°)
  python run_servo.py --angle 90  -> Đặt góc Servo về 90°

Cách dùng trong code Python:
  import run_servo
  run_servo.launch_bat()               # Bật chương trình bằng .bat
  run_servo.set_servo_angle(90)        # Điều khiển góc Servo
  data = run_servo.get_telemetry()     # Lấy thông số 4 LDR, Pin, Điện áp
"""

import os
import sys
import subprocess
import argparse
import time
from typing import Dict, Any, Optional

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

# Thêm thư mục Software vào sys.path nếu cần
current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir not in sys.path:
    sys.path.insert(0, current_dir)

from esp_comm import ESPCommunicator
import monitor
import wifi_setup
import main


def get_bat_path() -> str:
    """Tìm đường dẫn chính xác tới file SolarController.bat."""
    candidates = [
        os.path.join(current_dir, "SolarController.bat"),
        os.path.join(os.path.dirname(current_dir), "SolarController.bat"),
        os.path.join(os.path.dirname(os.path.dirname(current_dir)), "SolarController.bat"),
        os.path.join(os.path.dirname(os.path.dirname(current_dir)), "Software", "SolarController.bat"),
    ]
    for p in candidates:
        if os.path.exists(p):
            return p
    return candidates[2]


def launch_bat(new_window: bool = True) -> Any:
    """
    Gọi chạy chương trình SolarController bằng file .bat từ Python.
    :param new_window: Nếu True, mở cửa sổ cmd mới riêng biệt.
    """
    bat_path = get_bat_path()
    if not os.path.exists(bat_path):
        print(f"[LỖI] Không tìm thấy file .bat tại: {bat_path}")
        return None

    print(f"[INFO] Đang khởi chạy file .bat: {bat_path}")
    if sys.platform == "win32" and new_window:
        cmd = f'start "SolarController - Servo Monitor" cmd.exe /c "{bat_path}"'
        return subprocess.Popen(cmd, shell=True)
    else:
        return subprocess.run([bat_path], shell=True)


def set_servo_angle(angle: int, comm: Optional[ESPCommunicator] = None) -> bool:
    """Điều khiển góc xoay Servo (0° - 180°)."""
    close_after = False
    if comm is None:
        comm = ESPCommunicator()
        comm.start()
        close_after = True
        time.sleep(0.5)

    ok = comm.send_angle(angle)
    print(f"[SERVO] Đã gửi lệnh đặt góc Servo = {angle}° ({'Thành công' if ok else 'Thất bại'})")

    if close_after:
        time.sleep(0.5)
        comm.stop()
    return ok


def set_servo_mode(mode: str, comm: Optional[ESPCommunicator] = None) -> bool:
    """Chuyển chế độ Servo: 'AUTO' (tự bám sáng) hoặc 'MANUAL' (thủ công)."""
    close_after = False
    if comm is None:
        comm = ESPCommunicator()
        comm.start()
        close_after = True
        time.sleep(0.5)

    ok = comm.send_mode(mode)
    print(f"[SERVO] Chuyển chế độ Servo = {mode.upper()} ({'Thành công' if ok else 'Thất bại'})")

    if close_after:
        time.sleep(0.5)
        comm.stop()
    return ok


def test_servo_sweep(delay: float = 0.03, comm: Optional[ESPCommunicator] = None) -> bool:
    """Chạy thử nghiệm quét góc xoay Servo kiểm tra phần cứng."""
    close_after = False
    if comm is None:
        comm = ESPCommunicator()
        comm.start()
        close_after = True
        time.sleep(0.5)

    print("[SERVO] Bắt đầu quét kiểm tra động cơ Servo (0° -> 180° -> 90°)...")
    ok = comm.sweep_servo(delay=delay)
    print("[SERVO] Quét hoàn tất, động cơ đã trở về vị trí trung tâm 90°.")

    if close_after:
        time.sleep(0.5)
        comm.stop()
    return ok


def get_telemetry(comm: Optional[ESPCommunicator] = None) -> Dict[str, Any]:
    """Đọc toàn bộ thông số: 4 LDR, điện áp, pin, hướng góc xoay servo."""
    close_after = False
    if comm is None:
        comm = ESPCommunicator()
        comm.start()
        close_after = True
        time.sleep(1.0)

    data = comm.get_telemetry()
    if close_after:
        comm.stop()
    return data


def start_monitor():
    """Khởi động trực tiếp giao diện CLI Realtime Monitor."""
    monitor.run_monitor()


def start_wifi_setup():
    """Khởi động trực tiếp trình cài đặt WiFi cho ESP32."""
    wifi_setup.run_wifi_setup()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="SolarController Python Runner & Servo Manager")
    parser.add_argument("--bat", action="store_true", help="Bật chạy chương trình bằng file .bat")
    parser.add_argument("--monitor", action="store_true", help="Chạy giao diện hiển thị trạng thái tổng thể (Menu 1)")
    parser.add_argument("--wifi", action="store_true", help="Chạy trình cài đặt WiFi (Menu 2)")
    parser.add_argument("--sweep", action="store_true", help="Kiểm tra quét thử nghiệm động cơ Servo")
    parser.add_argument("--angle", type=int, help="Đặt góc quay Servo (0 - 180)")
    parser.add_argument("--auto", action="store_true", help="Bật chế độ Servo tự động bám sáng")
    parser.add_argument("--manual", action="store_true", help="Bật chế độ Servo điều khiển thủ công")

    args = parser.parse_args()

    if args.bat:
        launch_bat(new_window=True)
    elif args.monitor:
        start_monitor()
    elif args.wifi:
        start_wifi_setup()
    elif args.sweep:
        test_servo_sweep()
    elif args.angle is not None:
        set_servo_angle(args.angle)
    elif args.auto:
        set_servo_mode("AUTO")
    elif args.manual:
        set_servo_mode("MANUAL")
    else:
        # Mặc định: Chạy file .bat hoặc mở Menu chính
        bat_file = get_bat_path()
        if os.path.exists(bat_file):
            print("[INFO] Khởi chạy SolarController.bat...")
            launch_bat(new_window=False)
        else:
            main.main()
