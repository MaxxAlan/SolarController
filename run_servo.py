"""
SolarController - Root Python Launcher (run_servo.py)
Chuyển tiếp và gọi thực thi tới thư mục Code Base/python/run_servo.py
"""

import os
import sys

root_dir = os.path.dirname(os.path.abspath(__file__))
python_code_dir = os.path.join(root_dir, "Code Base", "python")
if python_code_dir not in sys.path:
    sys.path.insert(0, python_code_dir)

import importlib.util
target_script = os.path.join(python_code_dir, "run_servo.py")
spec = importlib.util.spec_from_file_location("codebase_run_servo", target_script)
if spec and spec.loader:
    codebase_run_servo = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(codebase_run_servo)
    
    # Expose các hàm chính ra ngoài namespace
    launch_bat = codebase_run_servo.launch_bat
    set_servo_angle = codebase_run_servo.set_servo_angle
    set_servo_mode = codebase_run_servo.set_servo_mode
    test_servo_sweep = codebase_run_servo.test_servo_sweep
    get_telemetry = codebase_run_servo.get_telemetry
    start_monitor = codebase_run_servo.start_monitor
    start_wifi_setup = codebase_run_servo.start_wifi_setup

if __name__ == "__main__":
    import subprocess
    cmd = [sys.executable, target_script] + sys.argv[1:]
    subprocess.run(cmd)
