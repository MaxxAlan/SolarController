"""
SolarController - Realtime CLI Monitor (monitor.py)
Hiển thị giao diện CLI thời gian thực cho:
  1. 4 module quang trở LDR (TL, TR, BL, BR, độ chênh lệch Diff, độ sáng Total)
  2. Điện áp tiêu thụ & trạng thái nguồn (Voltage, Vbat, sạc/xả)
  3. Lượng pin còn lại (Battery SOC %, thanh pin trực quan)
  4. Hướng của Solar Panel (servo continuous: TL/TR/BL/BR, hành động bám sáng)
Hỗ trợ phím tắt điều khiển trực tiếp: [Q] Thoát, [A] Auto, [M] Manual, [+/-] Nhịp quay.
"""

import sys
import os

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

script_dir = os.path.dirname(os.path.abspath(__file__))
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)

import time
from datetime import datetime
from typing import Optional

try:
    import msvcrt
    HAS_MSVCRT = True
except ImportError:
    HAS_MSVCRT = False

from rich.console import Console
from rich.live import Live
from rich.panel import Panel
from rich.table import Table
from rich.text import Text
from rich.align import Align
from rich.columns import Columns
from rich.layout import Layout
from rich import box

from esp_comm import ESPCommunicator


def create_progress_bar(val: float, max_val: float, width: int = 15, color: str = "green") -> str:
    """Tạo thanh đo đồ họa text trực quan."""
    ratio = max(0.0, min(1.0, val / max_val if max_val > 0 else 0.0))
    filled = int(round(ratio * width))
    empty = width - filled
    return f"[{color}]{'█' * filled}[/][dim]{'░' * empty}[/]"


def create_balance_bar(diff: int, max_range: int = 2000, width: int = 24) -> str:
    """Tạo thanh đo cân bằng ánh sáng Trái - Phải."""
    clamped = max(-max_range, min(max_range, diff))
    # Chuyển đổi từ [-max_range, max_range] sang [0, width]
    pos = int(round(((clamped + max_range) / (2 * max_range)) * width))
    pos = max(0, min(width, pos))
    
    chars = []
    center = width // 2
    for i in range(width + 1):
        if i == pos:
            chars.append("[bold yellow]●[/]")
        elif i == center:
            chars.append("[dim white]│[/]")
        else:
            chars.append("[dim cyan]─[/]")
    return "".join(chars)


def create_angle_bar(angle: int, width: int = 30) -> str:
    """Tạo thanh hiển thị hướng và góc xoay của Solar Panel (0° Bắc - 360° quay tròn)."""
    clamped = max(0, min(360, angle))
    pos = int(round((clamped / 360.0) * width))
    pos = max(0, min(width, pos))

    chars = []
    for i in range(width + 1):
        if i == pos:
            chars.append("[bold gold1]☀️[/]")
        elif i == width // 4:   # 90° Đông
            chars.append("[dim cyan]│[/]")
        elif i == width // 2:   # 180° Nam
            chars.append("[dim cyan]┼[/]")
        elif i == 3 * width // 4:  # 270° Tây
            chars.append("[dim cyan]│[/]")
        else:
            chars.append("[dim]─[/]")
    return "".join(chars)


def get_orientation_description(angle: int) -> str:
    """Quy đổi góc xoay Servo sang hướng địa lý thực tế (la bàn 360°)."""
    # Chuẩn hóa về 0-360
    a = angle % 360
    if a <= 15 or a >= 345:
        return "Bắc (North)"
    elif a < 75:
        return "Đông Bắc (Buổi sáng sớm)"
    elif a <= 105:
        return "Đông — Mặt trời mọc / Park đêm (90°)"
    elif a < 165:
        return "Đông Nam (Buổi sáng muộn)"
    elif a <= 195:
        return "Nam — Trưa Zenith (180°)"
    elif a < 255:
        return "Tây Nam (Đầu chiều)"
    elif a <= 285:
        return "Tây — Mặt trời lặn (270°)"
    else:
        return "Tây Bắc (Chiều tà)"


def build_dashboard_renderable(telemetry: dict, message: str = "") -> Layout:
    """Xây dựng giao diện CLI Dashboard bằng Rich Layout."""
    layout = Layout()
    layout.split_column(
        Layout(name="header", size=3),
        Layout(name="body", ratio=1),
        Layout(name="footer", size=3),
    )
    layout["body"].split_row(
        Layout(name="left_col", ratio=1),
        Layout(name="right_col", ratio=1),
    )
    layout["left_col"].split_column(
        Layout(name="ldr_matrix", ratio=1),
        Layout(name="optical_balance", size=7),
    )
    layout["right_col"].split_column(
        Layout(name="panel_orientation", ratio=1),
        Layout(name="power_system", ratio=1),
    )

    # =========================================================================
    # 1. HEADER
    # =========================================================================
    now_str = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    conn_src = telemetry.get("connection_source", "OFFLINE")
    is_conn = telemetry.get("is_connected", False)
    
    if is_conn:
        conn_tag = f"[bold green]● {conn_src}[/]"
    else:
        conn_tag = f"[bold red]○ {conn_src}[/]"

    wifi_info = f"WiFi: [cyan]{telemetry.get('wifi_ssid', '—')}[/]"
    if telemetry.get("wifi_ip"):
        wifi_info += f" | IP: [cyan]{telemetry.get('wifi_ip')}[/]"

    header_table = Table.grid(expand=True)
    header_table.add_column(justify="left", ratio=1)
    header_table.add_column(justify="center", ratio=2)
    header_table.add_column(justify="right", ratio=1)
    header_table.add_row(
        conn_tag,
        "[bold yellow]☀️  SOLAR CONTROLLER - GIÁM SÁT THỜI GIAN THỰC  ☀️[/]",
        f"[dim]{now_str}[/]"
    )
    layout["header"].update(Panel(header_table, box=box.ROUNDED, style="cyan"))

    # =========================================================================
    # 2. KHỐI CẢM BIẾN QUANG TRỞ (4× LDR)
    # =========================================================================
    tl = telemetry.get("ldr_tl", 0)
    tr = telemetry.get("ldr_tr", 0)
    bl = telemetry.get("ldr_bl", 0)
    br = telemetry.get("ldr_br", 0)

    # Tính milivolt và %
    def ldr_stat(raw):
        mv = int(raw * 3300 / 4095)
        pct = int(raw * 100 / 4095)
        bar = create_progress_bar(raw, 4095, width=10, color="bright_yellow")
        return f"{raw:4d} ADC ({mv/1000:.2f}V)\n{bar} [dim]{pct}%[/]"

    ldr_table = Table(expand=True, box=box.SIMPLE_HEAD, show_header=True)
    ldr_table.add_column("Vị trí Cảm Biến", style="bold cyan", width=14)
    ldr_table.add_column("Thông số Chi Tiết", justify="left")

    ldr_table.add_row("↖ Đông (TL)", ldr_stat(tl))
    ldr_table.add_row("↗ Tây  (TR)", ldr_stat(tr))
    ldr_table.add_row("↙ Nam  (BL)", ldr_stat(bl))
    ldr_table.add_row("↘ Bắc  (BR)", ldr_stat(br))

    layout["ldr_matrix"].update(
        Panel(ldr_table, title="[bold yellow]⚡ Ma Trận 4 Module Quang Trở (LDR Sensors)[/]", box=box.ROUNDED)
    )

    # =========================================================================
    # 3. CÂN BẰNG QUANG HỌC & ĐỘNG LỰC
    # =========================================================================
    diff = telemetry.get("diff", 0)
    total = telemetry.get("total", 0)
    light_state = telemetry.get("light_state", "DAY")
    
    bal_table = Table.grid(expand=True, padding=(0, 1))
    bal_table.add_column(ratio=1)

    # Thanh cân bằng
    bal_bar = create_balance_bar(diff, max_range=2000, width=22)
    bal_table.add_row(f"[bold cyan]Chênh Lệch Diff (Trái - Phải):[/] [bold yellow]{diff:+d}[/]")
    bal_table.add_row(f"  [dim]◀ Đông (-2000)[/] {bal_bar} [dim](+2000) Tây ▶[/]")
    
    # Đánh giá quang cảnh
    if light_state == "NIGHT" or total < 250:
        env_text = "[bold dark_orange]🌙 BAN ĐÊM[/] (Cường độ sáng thấp -> Đỗ Đông & Ngủ)"
    else:
        env_text = f"[bold green]☀️ BAN NGÀY[/] (Tổng độ sáng: [bold yellow]{total}[/])"
    
    bal_table.add_row(f"Môi trường: {env_text}")

    layout["optical_balance"].update(
        Panel(bal_table, title="[bold cyan]⚖️ Cân Bằng Ánh Sáng & Môi Trường[/]", box=box.ROUNDED)
    )

    # =========================================================================
    # 4. HƯỚNG CỦA SOLAR PANEL (ORIENTATION & SERVO)
    # =========================================================================
    # Servo continuous: không có góc thật -> hiển thị hướng đích + nhịp chạy gần nhất
    TARGET_ANGLE = {"TL": 90, "BL": 180, "TR": 270, "BR": 0}
    move_target = telemetry.get("move_target", "-")
    moving = telemetry.get("moving", 0)
    angle = TARGET_ANGLE.get(move_target, 90)
    mode = telemetry.get("mode", "AUTO")
    action = telemetry.get("action", "HOLD")
    
    orient_desc = get_orientation_description(angle)
    angle_bar = create_angle_bar(angle, width=28)
    last_target = telemetry.get("last_target", "-")
    last_ms = telemetry.get("last_ms", 0)

    # Định dạng hành động
    action_map = {
        "HOLD": "[bold green]HOLD (Cố Định - Bám Đúng Góc)[/]",
        "TRACK_CW": "[bold yellow]TRACK_CW (Quay Thuận Chiều Kim)[/]",
        "TRACK_CCW": "[bold yellow]TRACK_CCW (Quay Ngược Chiều Kim)[/]",
        "TRACK_L": "[bold yellow]TRACK_L (Quay Sang Trái)[/]",
        "TRACK_R": "[bold yellow]TRACK_R (Quay Sang Phải)[/]",
        "PARK_EAST": "[bold magenta]PARK_EAST (Đỗ Hướng Đông Đón Bình Minh)[/]",
        "SAFE_STOP": "[bold red]SAFE_STOP (Dừng Khẩn Cấp)[/]",
        "MANUAL": "[bold cyan]MANUAL (Người Dùng Điều Khiển)[/]",
        "INIT": "[dim]INIT (Khởi Tạo Hệ Thống)[/]"
    }
    action_display = action_map.get(action, f"[bold white]{action}[/]")
    mode_display = "[bold green]AUTO (Tự Động Bám Sáng)[/]" if mode == "AUTO" else "[bold cyan]MANUAL (Thủ Công)[/]"

    panel_table = Table.grid(expand=True, padding=(0, 1))
    panel_table.add_column(ratio=1)

    panel_table.add_row(f"Hướng Servo   : [bold gold1]{'▶ ' if moving else '■ '}{move_target}[/]  [dim]({orient_desc})[/]")
    panel_table.add_row(f"  [dim]Bắc 0°[/] {angle_bar} [dim]360°[/]")
    panel_table.add_row(f"[dim]Nhịp gần nhất: {last_target} = {last_ms} ms (continuous servo, không có góc thật)[/]")
    panel_table.add_row("")
    panel_table.add_row(f"Chế Độ Vận Hành: {mode_display}")
    panel_table.add_row(f"Hành Động Servo: {action_display}")

    layout["panel_orientation"].update(
        Panel(panel_table, title="[bold gold1]🧭 Hướng & Góc Xoay Tấm Pin (Solar Panel Orientation)[/]", box=box.ROUNDED)
    )

    # =========================================================================
    # 5. ĐIỆN ÁP TIÊU THỤ & LƯỢNG PIN CÒN LẠI
    # =========================================================================
    voltage = telemetry.get("voltage", telemetry.get("vbat", 3.85))
    soc = telemetry.get("soc", 75.0)
    charge = telemetry.get("charge", "IDLE")
    fault = telemetry.get("fault", 0)
    low_batt = telemetry.get("low_batt", 0)

    # Trạng thái sạc
    charge_map = {
        "CHARGING": "[bold green]⚡ ĐANG SẠC TỪ SOLAR PANEL[/]",
        "FULL": "[bold cyan]🔋 PIN ĐÃ ĐẦY (FULL CHARGED)[/]",
        "DISCHARGING": "[bold yellow]🔌 ĐANG XẢ NGUỒN (HỆ THỐNG HOẠT ĐỘNG)[/]",
        "IDLE": "[dim]💤 CHẾ ĐỘ CHỜ (IDLE)[/]"
    }
    charge_disp = charge_map.get(charge, charge)

    # Đánh giá điện áp
    if voltage < 3.2:
        v_status = "[bold red]⚠️ ĐIỆN ÁP NGUY HIỂM (<3.2V) - TỰ ĐỘNG BẢO VỆ PIN[/]"
    elif voltage < 3.6:
        v_status = "[yellow]Mức trung bình (Cần tiếp tục sạc)[/]"
    else:
        v_status = "[green]Mức danh định lý tưởng (3.7V - 4.2V)[/]"

    # Thanh đo dung lượng pin
    soc_color = "green" if soc >= 40 else ("yellow" if soc >= 20 else "red")
    soc_bar = create_progress_bar(soc, 100.0, width=18, color=soc_color)

    pwr_table = Table.grid(expand=True, padding=(0, 1))
    pwr_table.add_column(ratio=1)

    pwr_table.add_row(f"Lượng Pin Còn Lại : [bold {soc_color}]{soc:.1f}%[/]  {soc_bar}")
    pwr_table.add_row(f"Điện Áp Hệ Thống  : [bold bright_cyan]{voltage:.2f} V[/]  [dim]({v_status})[/]")
    pwr_table.add_row(f"Trạng Thái Sạc/Xả : {charge_disp}")

    alert_notes = []
    if fault: alert_notes.append("[bold red]CẢNH BÁO: LỖI CẢM BIẾN LDR[/]")
    if low_batt: alert_notes.append("[bold red]CẢNH BÁO: PIN YẾU - NGẮT ĐỘNG CƠ[/]")
    if not alert_notes: alert_notes.append("[dim green]Hệ thống nguồn & bảo vệ bình thường[/]")
    pwr_table.add_row(f"Cảnh Báo An Toàn  : {' | '.join(alert_notes)}")

    layout["power_system"].update(
        Panel(pwr_table, title="[bold bright_green]🔋 Điện Áp Tiêu Thụ & Lượng Pin Còn Lại[/]", box=box.ROUNDED)
    )

    # =========================================================================
    # 6. FOOTER (HOTKEYS)
    # =========================================================================
    footer_text = (
        "[bold cyan][Q][/] Menu  |  "
        "[bold cyan][A][/] Auto  |  "
        "[bold cyan][M][/] Manual  |  "
        "[bold cyan][+][/]/[bold cyan][-][/] CW/CCW  |  "
        "[bold cyan][0][/][bold cyan][9][/][bold cyan][6][/][bold cyan][8][/] Bắc/Đông/Nam/Tây  |  "
        "[bold cyan][T][/] Test Servo  |  "
        "[bold cyan][P][/] Refresh"
    )
    if message:
        footer_text = f"[bold yellow]{message}[/]  |  " + footer_text

    layout["footer"].update(Panel(Align.center(footer_text), box=box.ROUNDED, style="blue"))

    return layout


def run_monitor(comm: Optional[ESPCommunicator] = None):
    """Vòng lặp chạy giao diện Realtime Monitor."""
    own_comm = False
    if comm is None:
        comm = ESPCommunicator()
        comm.start()
        own_comm = True

    console = Console()
    console.clear()

    # Chế độ demo nếu không có phần cứng
    demo_mode = False
    demo_angle = 90
    status_message = ""
    status_msg_time = 0.0

    try:
        with Live(console=console, refresh_per_second=4, screen=True) as live:
            while True:
                # 1. Đọc phím bấm không chặn (Non-blocking keyboard input)
                if HAS_MSVCRT and msvcrt.kbhit():
                    key = msvcrt.getch()
                    # Phím mũi tên hoặc phím mở rộng
                    if key in (b'\x00', b'\xe0'):
                        arrow = msvcrt.getch()
                        if arrow == b'H': # Up
                            key = b'+'
                        elif arrow == b'P': # Down
                            key = b'-'

                    if key in (b'q', b'Q', b'\x1b'):
                        break
                    elif key in (b'a', b'A'):
                        comm.send_mode("AUTO")
                        status_message = "Servo: Chế độ AUTO (Tự động bám sáng)"
                        status_msg_time = time.time()
                    elif key in (b'm', b'M'):
                        comm.send_mode("MANUAL")
                        status_message = "Servo: Chế độ MANUAL (Điều khiển thủ công)"
                        status_msg_time = time.time()
                    elif key in (b'+', b'='):
                        comm.pulse_servo("CW", 200)
                        status_message = "Servo Manual: quay CW 200 ms"
                        status_msg_time = time.time()
                    elif key in (b'-', b'_'):
                        comm.pulse_servo("CCW", 200)
                        status_message = "Servo Manual: quay CCW 200 ms"
                        status_msg_time = time.time()
                    elif key == b'0':
                        comm.send_angle(0)
                        status_message = "Servo: Hướng Bắc (0°)"
                        status_msg_time = time.time()
                    elif key == b'9':
                        comm.send_angle(90)
                        status_message = "Servo: Hướng Đông / Park (90°)"
                        status_msg_time = time.time()
                    elif key == b'6':
                        comm.send_angle(180)
                        status_message = "Servo: Hướng Nam (180°)"
                        status_msg_time = time.time()
                    elif key == b'8':
                        comm.send_angle(270)
                        status_message = "Servo: Hướng Tây (270°)"
                        status_msg_time = time.time()
                    elif key in (b't', b'T'):
                        status_message = "Đang quét kiểm tra Servo (Đông → Nam → Tây → Bắc → Đông)..."
                        status_msg_time = time.time()
                        import threading
                        threading.Thread(target=comm.sweep_servo, daemon=True).start()
                    elif key in (b'p', b'P', b'r', b'R'):
                        comm.send_serial_cmd("p")
                        status_message = "Đã gửi yêu cầu refresh trạng thái"
                        status_msg_time = time.time()

                # 2. Xóa message sau 3s
                if time.time() - status_msg_time > 3.0:
                    status_message = ""

                # 3. Lấy telemetry và render
                telemetry = comm.get_telemetry()
                layout = build_dashboard_renderable(telemetry, status_message)
                live.update(layout)

                time.sleep(0.2)

    except KeyboardInterrupt:
        pass
    finally:
        if own_comm:
            comm.stop()
        console.clear()


if __name__ == "__main__":
    run_monitor()
