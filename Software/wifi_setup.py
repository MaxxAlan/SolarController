"""
SolarController - WiFi Configuration Wizard (wifi_setup.py)
Cài đặt WiFi cho vi điều khiển ESP32:
  - Cho phép nhập SSID & Mật khẩu WiFi mới.
  - Tự động gửi lệnh qua Serial (wifi|<ssid>|<pass>) và Cloud Firebase.
  - Lưu vào bộ nhớ Flash NVS của ESP32 (không mất khi mất điện).
  - Tự động khôi phục WiFi cũ nếu kết nối không thành công.
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
from typing import Optional

from rich.console import Console
from rich.panel import Panel
from rich.table import Table
from rich.prompt import Prompt, Confirm
from rich.progress import Progress, SpinnerColumn, TextColumn, BarColumn, TimeRemainingColumn
from rich import box

from esp_comm import ESPCommunicator


def run_wifi_setup(comm: Optional[ESPCommunicator] = None):
    """Trình hướng dẫn cài đặt WiFi cho ESP32."""
    own_comm = False
    if comm is None:
        comm = ESPCommunicator()
        comm.start()
        own_comm = True

    console = Console()
    console.clear()

    # Chờ 1 giây để đọc trạng thái hiện tại
    time.sleep(1.0)
    current_data = comm.get_telemetry()
    cur_ssid = current_data.get("wifi_ssid", "—")
    cur_ip = current_data.get("wifi_ip", "Chưa có IP")
    conn_src = current_data.get("connection_source", "OFFLINE")

    # Bảng thông tin hiện tại
    info_table = Table(expand=True, box=box.ROUNDED, show_header=False)
    info_table.add_column("Thuộc tính", style="bold cyan", width=24)
    info_table.add_column("Giá trị hiện tại", style="bold white")

    info_table.add_row("Kênh kết nối", f"[yellow]{conn_src}[/]")
    info_table.add_row("Tên WiFi (SSID) hiện tại", f"[green]{cur_ssid}[/]")
    info_table.add_row("Địa chỉ IP hiện tại", f"[green]{cur_ip}[/]")
    info_table.add_row("Cơ chế bảo vệ", "[dim]Tự động lưu Flash NVS | Tự phục hồi mạng cũ nếu sai pass[/]")

    console.print(Panel(
        info_table,
        title="[bold yellow]📡 CÀI ĐẶT CẤU HÌNH WIFI CHO ESP32 (WIFI SETUP)[/]",
        subtitle="[dim]SolarController IoT v1.1.0[/]",
        box=box.DOUBLE
    ))
    console.print()

    # Nhập thông tin WiFi mới
    console.print("[bold yellow]Nhập thông tin mạng Wi-Fi 2.4GHz bạn muốn kết nối:[/]")
    ssid = Prompt.ask("  [bold cyan]1. Tên WiFi (SSID)[/]").strip()

    if not ssid:
        console.print("[bold red]❌ Tên WiFi không được để trống! Đã hủy cài đặt.[/]")
        console.input("\n[dim]Nhấn Enter để quay lại Menu...[/]")
        if own_comm: comm.stop()
        return

    password = Prompt.ask("  [bold cyan]2. Mật khẩu WiFi (Password)[/]", default="", show_default=False).strip()

    console.print()
    confirm = Confirm.ask(f"  [bold yellow]Xác nhận gửi WiFi '[bold green]{ssid}[/]' xuống ESP32?[/]", default=True)
    if not confirm:
        console.print("[bold yellow]Đã hủy thao tác cài đặt WiFi.[/]")
        console.input("\n[dim]Nhấn Enter để quay lại Menu...[/]")
        if own_comm: comm.stop()
        return

    console.print()
    console.print(f"[bold cyan]Đang truyền cấu hình WiFi mới xuống ESP32...[/]")
    res = comm.send_wifi_credentials(ssid, password)
    console.print(f"  [green]✔ {res['message']}[/]")

    # Đợi phản hồi kết nối từ ESP32 (tối đa 20 giây)
    console.print()
    console.print("[dim]ESP32 đang áp dụng cấu hình và kết nối vào mạng mới (tối đa 20s)...[/]")

    connected_ok = False
    new_ip = ""

    with Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}"),
        BarColumn(),
        TimeRemainingColumn(),
        console=console,
        transient=False
    ) as progress:
        task = progress.add_task("[cyan]Đang đợi ESP32 phản hồi địa chỉ IP mới...", total=20)
        
        for _ in range(40):
            time.sleep(0.5)
            progress.update(task, advance=0.5)
            
            # Kiểm tra feedback từ Serial hoặc Cloud
            if comm.last_wifi_feedback and "APPLY OK" in comm.last_wifi_feedback:
                connected_ok = True
                break

            data = comm.get_telemetry()
            if data.get("wifi_ssid") == ssid and data.get("wifi_ip"):
                connected_ok = True
                new_ip = data.get("wifi_ip")
                break

    console.print()
    if connected_ok:
        ip_display = new_ip or comm.telemetry.get("wifi_ip", "Đã nhận IP")
        console.print(Panel(
            f"[bold green]🎉 CÀI ĐẶT WIFI THÀNH CÔNG![/]\n\n"
            f"  • Mạng WiFi đã kết nối : [bold cyan]{ssid}[/]\n"
            f"  • Địa chỉ IP của ESP32 : [bold yellow]{ip_display}[/]\n"
            f"  • Trạng thái Flash NVS : [green]Đã lưu vĩnh viễn vào bộ nhớ ESP32[/]",
            title="[bold green]THÀNH CÔNG[/]",
            box=box.ROUNDED
        ))
    else:
        console.print(Panel(
            "[bold yellow]⚠️ ĐÃ GỬI LỆNH CẤU HÌNH NHƯNG CHƯA NHẬN ĐƯỢC PHẢN HỒI IP MỚI[/]\n\n"
            "Các trường hợp có thể xảy ra:\n"
            "  1. Sai mật khẩu WiFi: ESP32 sẽ tự động hủy và khôi phục lại WiFi cũ.\n"
            "  2. Mạng 5GHz: ESP32 chỉ hỗ trợ băng tần WiFi 2.4GHz.\n"
            "  3. ESP32 đang khởi động lại và kết nối, bạn có thể kiểm tra lại trong màn hình Giám sát.",
            title="[bold yellow]LƯU Ý[/]",
            box=box.ROUNDED
        ))

    console.input("\n[dim]Nhấn Enter để quay lại Menu chính...[/]")
    if own_comm:
        comm.stop()


if __name__ == "__main__":
    run_wifi_setup()
