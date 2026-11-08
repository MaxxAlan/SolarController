import docx
from docx.shared import Inches, Pt, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
import shutil
import os

docx_path = r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Document\Report\Group1_SolarTracker_Report.docx"
backup_path = r"D:\project\SolarController\cache\Group1_SolarTracker_Report_backup.docx"
shutil.copy2(docx_path, backup_path)
print(f"Backed up original docx to {backup_path}")

doc = docx.Document(docx_path)

# 1. Update Header: Authors and Emails
# P3: Authors
p_auth = doc.paragraphs[3]
p_auth.text = ""
r_auth = p_auth.add_run("1st Hoàng Tiến Mạnh, 2nd Võ Đặng Anh Khoa, 3rd Đặng Thị Phương Anh, 4th Nguyễn Thành Luân, and Duc Ngoc Minh Dang")
r_auth.font.name = "Times New Roman"
r_auth.font.size = Pt(11)
p_auth.alignment = WD_ALIGN_PARAGRAPH.CENTER

# P5: Emails
p_email = doc.paragraphs[5]
p_email.text = ""
r_email = p_email.add_run("{manhhtse203727, khoavdse203865, anhdtpse203874, luanntse184179}@fpt.edu.vn, and ducdnm2@fe.edu.vn")
r_email.font.name = "Times New Roman"
r_email.font.size = Pt(9)
r_email.font.italic = True
p_email.alignment = WD_ALIGN_PARAGRAPH.CENTER

# 2. Add Index Terms after Abstract (P7)
# Let's insert a paragraph before P8 (I. INTRODUCTION)
p_intro = doc.paragraphs[8]
p_index = p_intro.insert_paragraph_before()
p_index.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
r_it1 = p_index.add_run("Index Terms— ")
r_it1.font.name = "Times New Roman"
r_it1.font.size = Pt(10)
r_it1.font.bold = True
r_it2 = p_index.add_run("Dual-axis solar tracking, Internet of Things (IoT), Arduino Uno, ESP32-S3, Firebase Realtime Database, Photovoltaic energy harvesting, Battery management system (BMS), Hardware hysteresis.")
r_it2.font.name = "Times New Roman"
r_it2.font.size = Pt(10)

# Add an empty paragraph after index terms for spacing
p_space = p_intro.insert_paragraph_before()
p_space.text = ""

# 3. Update Table II (Pin-to-pin Arduino Uno)
table2 = doc.tables[1]
for row in table2.rows:
    cell_pin = row.cells[0].text.strip()
    if cell_pin == "D9":
        row.cells[3].text = "PWM signal controlling azimuth rotation (0° - 150°)"
    elif cell_pin == "D10":
        row.cells[3].text = "PWM signal controlling elevation rotation (0° - 90°)"

print("Updated Table II servo ranges to 0-150° (Pan) and 0-90° (Tilt).")

# Save intermediate
doc.save(docx_path)
print("Saved intermediate docx.")
