"""
SolarController - Communication Layer (esp_comm.py)
Ho tro giao tiep da kenh (Dual-Channel):
  1. Serial UART (Direct USB COM Port - 115200 baud)
  2. Cloud Realtime Database (Firebase RTDB REST API)
Tu dong nhan dien kenh du lieu va dong bo hai chieu.
"""

import os
import sys
import json
import time
import re
import threading
from typing import Dict, Any, Optional

try:
    import serial
    import serial.tools.list_ports
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False

try:
    import requests
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False


class ESPCommunicator:
    def __init__(self, config_path: Optional[str] = None):
        if config_path is None:
            base_dir = os.path.dirname(os.path.abspath(__file__))
            config_path = os.path.join(base_dir, "config.json")
        
        self.config_path = config_path
        self.config = self._load_config()

        # Telemetry State
        self._lock = threading.Lock()
        self.telemetry: Dict[str, Any] = {
            "ldr_tl": 0,
            "ldr_tr": 0,
            "ldr_bl": 0,
            "ldr_br": 0,
            "diff": 0,
            "total": 0,
            "light_state": "DAY",
            "mode": "AUTO",
            "action": "INIT",
            "angle": 90,
            "servo_on": 1,
            "voltage": 3.85,
            "vbat": 3.85,
            "soc": 75.0,
            "charge": "IDLE",
            "fault": 0,
            "low_batt": 0,
            "offline": 0,
            "wifi_ssid": "hoangmanh",
            "wifi_ip": "",
            "rssi": -60,
            "uptime_s": 0,
            "connection_source": "OFFLINE",
            "is_connected": False,
            "last_updated": 0.0,
            "last_serial_rx": 0.0,
            "last_cloud_rx": 0.0,
        }

        # Serial parameters
        self.ser: Optional[Any] = None
        self.active_com_port = ""
        self.serial_thread: Optional[threading.Thread] = None
        self.cloud_thread: Optional[threading.Thread] = None
        self.running = False
        self.last_wifi_feedback: Optional[str] = None

    def _load_config(self) -> dict:
        default_cfg = {
            "serial": {
                "port": "COM5",
                "baudrate": 115200,
                "timeout": 1.0,
                "auto_detect": True
            },
            "firebase": {
                "url": "https://solarcontroller-1cc93-default-rtdb.asia-southeast1.firebasedatabase.app",
                "auth": "XURINmjqSrUgYAs1B7TjQbZoe8Nn151O8BWGRKYs",
                "telemetry_path": "/solar_controller/telemetry",
                "command_path": "/solar_controller/command",
                "config_path": "/solar_controller/config",
                "enabled": True
            },
            "monitor": {
                "refresh_rate_hz": 2.0,
                "stale_timeout_sec": 8.0
            }
        }
        if os.path.exists(self.config_path):
            try:
                with open(self.config_path, "r", encoding="utf-8") as f:
                    cfg = json.load(f)
                    return cfg
            except Exception as e:
                print(f"[WARN] Khong the doc config.json: {e}, su dung cau hinh mac dinh.")
        return default_cfg

    def find_esp_port(self) -> str:
        """Tu dong quet va tim cong COM cua ESP32."""
        if not HAS_SERIAL:
            return ""
        ports = list(serial.tools.list_ports.comports())
        if not ports:
            return ""
        
        # Uu tien cong phat hien chip USB-UART pho bien cua ESP32
        keywords = ["CH343", "CH340", "CP210", "FTDI", "UART", "ESP32", "USB Serial"]
        for p in ports:
            desc = f"{p.device} {p.description} {p.manufacturer or ''}".upper()
            for kw in keywords:
                if kw in desc:
                    return p.device

        # Neu da chi dinh cong trong config va cong do dang ton tai
        target_port = self.config.get("serial", {}).get("port", "COM5")
        for p in ports:
            if p.device.upper() == target_port.upper():
                return p.device

        # Neu khong, chon cong COM dau tien
        return ports[0].device

    def start(self):
        """Khoi chay cac luong giao tiep nen."""
        self.running = True

        # Khoi chay luong Serial
        if HAS_SERIAL:
            self.serial_thread = threading.Thread(target=self._serial_worker, daemon=True)
            self.serial_thread.start()

        # Khoi chay luong Firebase Cloud
        if HAS_REQUESTS and self.config.get("firebase", {}).get("enabled", True):
            self.cloud_thread = threading.Thread(target=self._cloud_worker, daemon=True)
            self.cloud_thread.start()

    def stop(self):
        """Dung giao tiep va giai phong tai nguyen."""
        self.running = False
        if self.ser and hasattr(self.ser, "is_open") and self.ser.is_open:
            try:
                self.ser.close()
            except Exception:
                pass

    # =========================================================================
    # SERIAL WORKER
    # =========================================================================
    def _serial_worker(self):
        baud = self.config.get("serial", {}).get("baudrate", 115200)
        timeout = self.config.get("serial", {}).get("timeout", 1.0)

        while self.running:
            port = self.find_esp_port()
            if not port:
                time.sleep(2.0)
                continue

            try:
                self.ser = serial.Serial()
                self.ser.port = port
                self.ser.baudrate = baud
                self.ser.timeout = timeout
                # Tranh vo tinh giu chan reset ESP32 qua DTR/RTS
                self.ser.dtr = False
                self.ser.rts = False
                self.ser.open()
                self.active_com_port = port

                # Gui lenh p de lay trang thai ban dau
                time.sleep(0.5)
                self.ser.write(b"p\r\n")

                while self.running and self.ser.is_open:
                    line = self.ser.readline().decode("utf-8", errors="ignore").strip()
                    if not line:
                        continue
                    self._parse_serial_line(line)

            except Exception:
                pass
            finally:
                if self.ser and hasattr(self.ser, "is_open") and self.ser.is_open:
                    try:
                        self.ser.close()
                    except Exception:
                        pass
                self.ser = None
                time.sleep(2.0)

    def _parse_serial_line(self, line: str):
        now = time.time()
        with self._lock:
            self.telemetry["last_serial_rx"] = now
            self.telemetry["connection_source"] = f"SERIAL ({self.active_com_port})"
            self.telemetry["is_connected"] = True
            self.telemetry["last_updated"] = now

            # 1. Parsing line: MODE=%s ANGLE=%d ACTION=%s STATE=%s
            m1 = re.search(r"MODE=(\w+)\s+ANGLE=(\d+)\s+ACTION=(\w+)\s+STATE=(\w+)", line)
            if m1:
                self.telemetry["mode"] = m1.group(1).upper()
                self.telemetry["angle"] = int(m1.group(2))
                self.telemetry["action"] = m1.group(3).upper()
                self.telemetry["light_state"] = m1.group(4).upper()
                return

            # 2. Parsing line: LDR TL=%d TR=%d BL=%d BR=%d | DIFF=%d TOTAL=%d
            m2 = re.search(r"LDR\s+TL=(\d+)\s+TR=(\d+)\s+BL=(\d+)\s+BR=(\d+)\s+\|\s+DIFF=([+-]?\d+)\s+TOTAL=(\d+)", line)
            if m2:
                self.telemetry["ldr_tl"] = int(m2.group(1))
                self.telemetry["ldr_tr"] = int(m2.group(2))
                self.telemetry["ldr_bl"] = int(m2.group(3))
                self.telemetry["ldr_br"] = int(m2.group(4))
                self.telemetry["diff"] = int(m2.group(5))
                self.telemetry["total"] = int(m2.group(6))
                return

            # 3. Parsing line: VBAT=%.2fV SOC=%.0f% CHARGE=%s FAULT=%d LOWBATT=%d NET=%s
            m3 = re.search(r"VBAT=([0-9.]+)V\s+SOC=([0-9.]+)%\s+CHARGE=(\w+)\s+FAULT=(\d+)\s+LOWBATT=(\d+)\s+NET=(\w+)", line)
            if m3:
                self.telemetry["voltage"] = float(m3.group(1))
                self.telemetry["vbat"] = float(m3.group(1))
                self.telemetry["soc"] = float(m3.group(2))
                self.telemetry["charge"] = m3.group(3).upper()
                self.telemetry["fault"] = int(m3.group(4))
                self.telemetry["low_batt"] = int(m3.group(5))
                self.telemetry["offline"] = 1 if m3.group(6) == "OFFLINE" else 0
                return

            # 4. Parsing WiFi responses
            if "[WIFI]" in line:
                self.last_wifi_feedback = line
                if "OK IP=" in line:
                    ip_match = re.search(r"IP=([0-9.]+)", line)
                    if ip_match:
                        self.telemetry["wifi_ip"] = ip_match.group(1)
                elif "APPLY OK ip=" in line:
                    ip_match = re.search(r"ip=([0-9.]+)", line)
                    if ip_match:
                        self.telemetry["wifi_ip"] = ip_match.group(1)

    # =========================================================================
    # CLOUD WORKER (FIREBASE RTDB)
    # =========================================================================
    def _cloud_worker(self):
        fb_url = self.config.get("firebase", {}).get("url", "").rstrip("/")
        auth = self.config.get("firebase", {}).get("auth", "")
        t_path = self.config.get("firebase", {}).get("telemetry_path", "/solar_controller/telemetry")
        poll_url = f"{fb_url}{t_path}.json"
        if auth:
            poll_url += f"?auth={auth}"

        while self.running:
            try:
                res = requests.get(poll_url, timeout=2.5)
                if res.status_code == 200:
                    data = res.json()
                    if isinstance(data, dict):
                        self._apply_cloud_telemetry(data)
            except Exception:
                pass
            time.sleep(1.0)

    def _apply_cloud_telemetry(self, data: dict):
        now = time.time()
        with self._lock:
            self.telemetry["last_cloud_rx"] = now
            # Neu Serial khong hoat dong trong 3s qua, Cloud tro thanh nguon chinh
            if (now - self.telemetry["last_serial_rx"]) > 3.0:
                self.telemetry["connection_source"] = "CLOUD (Firebase RTDB)"
                self.telemetry["is_connected"] = True
                self.telemetry["last_updated"] = now

            for key in ["ldr_tl", "ldr_tr", "ldr_bl", "ldr_br", "diff", "total",
                        "angle", "mode", "action", "voltage", "vbat", "soc",
                        "charge", "fault", "low_batt", "light_state", "wifi_ssid",
                        "wifi_ip", "rssi", "uptime_s"]:
                if key in data and data[key] is not None:
                    self.telemetry[key] = data[key]

            # Alias logic
            if "voltage" in data and "vbat" not in data:
                self.telemetry["vbat"] = data["voltage"]
            elif "vbat" in data and "voltage" not in data:
                self.telemetry["voltage"] = data["vbat"]

    # =========================================================================
    # PUBLIC API
    # =========================================================================
    def get_telemetry(self) -> Dict[str, Any]:
        """Lay snapshot telemetry an toan."""
        now = time.time()
        stale_sec = self.config.get("monitor", {}).get("stale_timeout_sec", 8.0)
        with self._lock:
            data = dict(self.telemetry)
            # Kiem tra xem du lieu co bi stale (mat ket noi) khong
            if (now - data["last_updated"]) > stale_sec and data["last_updated"] > 0:
                data["is_connected"] = False
                data["connection_source"] = "MAT KET NOI (DISCONNECTED)"
            elif data["last_updated"] == 0:
                data["is_connected"] = False
                data["connection_source"] = "DANG CHO TIN HIEU..."
            return data

    def send_serial_cmd(self, cmd: str) -> bool:
        """Gui lenh thô qua cong Serial."""
        if self.ser and hasattr(self.ser, "is_open") and self.ser.is_open:
            try:
                line = (cmd.strip() + "\r\n").encode("utf-8")
                self.ser.write(line)
                self.ser.flush()
                return True
            except Exception:
                return False
        return False

    def send_cloud_command(self, payload: dict) -> bool:
        """Gui lenh len Firebase RTDB node /solar_controller/command."""
        if not HAS_REQUESTS:
            return False
        fb_url = self.config.get("firebase", {}).get("url", "").rstrip("/")
        auth = self.config.get("firebase", {}).get("auth", "")
        cmd_path = self.config.get("firebase", {}).get("command_path", "/solar_controller/command")
        target_url = f"{fb_url}{cmd_path}.json"
        if auth:
            target_url += f"?auth={auth}"

        try:
            res = requests.patch(target_url, json=payload, timeout=4.0)
            return res.status_code == 200
        except Exception:
            return False

    def send_mode(self, mode: str) -> bool:
        """Chuyen che do AUTO hoac MANUAL."""
        mode_upper = mode.upper()
        serial_ok = False
        if mode_upper == "AUTO":
            serial_ok = self.send_serial_cmd("a")
        else:
            serial_ok = self.send_serial_cmd("m")

        cloud_ok = self.send_cloud_command({"mode": mode_upper})
        return serial_ok or cloud_ok

    def send_angle(self, angle: int) -> bool:
        """Dat goc quay cho servo trong che do MANUAL (0 - 180)."""
        angle = max(0, min(180, angle))
        serial_ok = self.send_serial_cmd(f"angle {angle}")
        cloud_ok = self.send_cloud_command({"mode": "MANUAL", "angle": angle})
        return serial_ok or cloud_ok

    def sweep_servo(self, delay: float = 0.03, step: int = 5) -> bool:
        """Quét thử góc servo từ 0° -> 180° -> 90° để kiểm tra động cơ panel."""
        self.send_mode("MANUAL")
        for a in range(0, 181, step):
            self.send_angle(a)
            time.sleep(delay)
        for a in range(180, 89, -step):
            self.send_angle(a)
            time.sleep(delay)
        return True

    def send_wifi_credentials(self, ssid: str, password: str) -> Dict[str, Any]:
        """
        Gui thong tin WiFi moi xuong ESP32:
        - Qua Serial: wifi|<ssid>|<pass>
        - Qua Firebase: {wifi_ssid, wifi_pass}
        """
        ssid = ssid.strip()
        result = {
            "serial_sent": False,
            "cloud_sent": False,
            "success": False,
            "message": ""
        }

        if not ssid:
            result["message"] = "Tên WiFi (SSID) không được để trống!"
            return result

        # 1. Gui qua Serial neu co
        cmd_serial = f"wifi|{ssid}|{password}"
        result["serial_sent"] = self.send_serial_cmd(cmd_serial)

        # 2. Gui qua Cloud Firebase
        payload = {"wifi_ssid": ssid, "wifi_pass": password}
        result["cloud_sent"] = self.send_cloud_command(payload)

        if result["serial_sent"] or result["cloud_sent"]:
            result["success"] = True
            channels = []
            if result["serial_sent"]: channels.append("Cổng Serial USB")
            if result["cloud_sent"]: channels.append("Cloud Firebase")
            result["message"] = f"Đã gửi cấu hình WiFi qua: {', '.join(channels)}."
        else:
            result["message"] = "Không thể gửi lệnh qua Serial lẫn Cloud. Vui lòng kiểm tra cáp USB hoặc mạng Internet!"

        return result
