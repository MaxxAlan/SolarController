"""
SolarController - Main CLI Application (main.py)
Điểm khởi chạy chính của ứng dụng giao tiếp SolarController:
  [1] Hiển thị trạng thái tổng thể (Realtime CLI Monitor)
  [2] Cài đặt WiFi cho ESP32 (Setting WiFi)
  [3] Thoát chương trình
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

import argparse
import time

from rich.console import Console
from rich.panel import Panel
from rich.table import Table
from rich.prompt import Prompt
from rich import box

from esp_comm import ESPCommunicator
import monitor
import wifi_setup


def show_banner(console: Console, comm: ESPCommunicator):
    """Hiển thị banner hệ thống và thông tin kết nối."""
    console.clear()
    
    data = comm.get_telemetry()
    conn_src = data.get("connection_source", "OFFLINE")
    is_conn = data.get("is_connected", False)
    status_color = "green" if is_conn else "yellow"

    info_grid = Table.grid(expand=True, padding=(0, 2))
    info_grid.add_column(ratio=1)
    info_grid.add_column(ratio=1)

    info_grid.add_row(
        f"Kênh kết nối : [{status_color}]● {conn_src}[/]",
        f"Mạng WiFi    : [cyan]{data.get('wifi_ssid', '—')}[/]"
    )
    info_grid.add_row(
        f"Cổng Serial  : [yellow]{comm.active_com_port or 'Chưa kết nối COM'}[/]",
        f"Địa chỉ IP   : [cyan]{data.get('wifi_ip') or 'Chưa nhận IP'}[/]"
    )

    menu_panel = Panel(
        info_grid,
        title="[bold yellow]☀️  HỆ THỐNG ĐIỀU KHIỂN & GIÁM SÁT PIN MẶT TRỜI (SOLAR CONTROLLER)  ☀️[/]",
        subtitle="[dim]ESP32-S3 IoT Telemetry & Control Suite v1.1.0[/]",
        box=box.DOUBLE,
        style="cyan"
    )
    console.print(menu_panel)
    console.print()


def show_menu(console: Console) -> str:
    """Hiển thị menu lựa chọn và nhận input từ người dùng."""
    menu_table = Table(expand=True, box=box.ROUNDED, show_header=False)
    menu_table.add_column("Phím", style="bold yellow", width=10, justify="center")
    menu_table.add_column("Chức Năng", style="bold white")
    menu_table.add_column("Mô Tả Chi Tiết", style="dim")

    menu_table.add_row(
        "[1]",
        "Hiển thị trạng thái tổng thể",
        "Giao diện CLI Realtime: 4 LDR, điện áp, lượng pin còn lại, hướng panel"
    )
    menu_table.add_row(
        "[2]",
        "Cài đặt WiFi cho ESP32",
        "Setting WiFi: Cấu hình SSID & Mật khẩu mới, tự lưu NVS Flash"
    )
    menu_table.add_row(
        "[3]",
        "Thoát chương trình",
        "Đóng kết nối Serial/Cloud và kết thúc"
    )

    console.print(Panel(menu_table, title="[bold white]DANH MỤC CHỨC NĂNG (MENU CHÍNH)[/]", box=box.ROUNDED))
    console.print()

    choice = Prompt.ask("👉 [bold yellow]Vui lòng chọn chức năng (1-3)[/]", choices=["1", "2", "3", "q", "Q"], default="1")
    return choice


def main():
    parser = argparse.ArgumentParser(description="SolarController CLI Software")
    parser.add_argument("--action", choices=["monitor", "wifi", "menu"], default="menu",
                        help="Chức năng chạy trực tiếp: monitor (1), wifi (2), menu")
    args = parser.parse_args()

    console = Console()

    # Khởi tạo bộ giao tiếp dùng chung
    comm = ESPCommunicator()
    comm.start()

    try:
        # Nếu được gọi trực tiếp bằng tham số từ .bat hoặc dòng lệnh
        if args.action == "monitor":
            monitor.run_monitor(comm)
            return
        elif args.action == "wifi":
            wifi_setup.run_wifi_setup(comm)
            return

        # Vòng lặp Menu chính
        while True:
            show_banner(console, comm)
            choice = show_menu(console)

            if choice == "1":
                monitor.run_monitor(comm)
            elif choice == "2":
                wifi_setup.run_wifi_setup(comm)
            elif choice in ("3", "q", "Q"):
                console.print()
                console.print("[bold green]Cảm ơn bạn đã sử dụng SolarController! Đang thoát...[/]")
                break

    except KeyboardInterrupt:
        console.print("\n[yellow]Đã nhận tín hiệu ngắt (Ctrl+C). Thoát chương trình...[/]")
    finally:
        comm.stop()


if __name__ == "__main__":
    main()
