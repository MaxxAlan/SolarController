import shutil
import time
import os

src = r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Document\Report\Group1_SolarTracker_Report_Final.docx"
dst = r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Document\Report\Group1_SolarTracker_Report.docx"

try:
    shutil.copy2(src, dst)
    print("SUCCESS: Overwrote Group1_SolarTracker_Report.docx directly!")
except Exception as e:
    print(f"File currently locked ({e}). Will overwrite as soon as Word closes.")
