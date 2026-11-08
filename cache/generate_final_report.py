import docx
from docx.shared import Inches, Pt
from docx.enum.text import WD_ALIGN_PARAGRAPH
import os

source_docx = r"D:\project\SolarController\cache\Group1_SolarTracker_Report_backup.docx"
target_docx = r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Document\Report\Group1_SolarTracker_Report_Final.docx"
img_dir = r"D:\project\SolarController\img"

doc = docx.Document(source_docx)

# ==============================================================================
# 1. HEADER UPDATES
# ==============================================================================
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

# ==============================================================================
# 2. INDEX TERMS (KEYWORDS)
# ==============================================================================
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

p_space = p_intro.insert_paragraph_before()
p_space.text = ""

# ==============================================================================
# 3. TABLE II (PIN INTERFACING - SERVO RANGES)
# ==============================================================================
table2 = doc.tables[1]
for row in table2.rows:
    cell_pin = row.cells[0].text.strip()
    if cell_pin == "D9":
        row.cells[3].text = "PWM signal controlling azimuth rotation (0° - 150°)"
    elif cell_pin == "D10":
        row.cells[3].text = "PWM signal controlling elevation rotation (0° - 90°)"

# ==============================================================================
# 4. SECTION III.A PROTOTYPE FIGURES & DESCRIPTIONS
# ==============================================================================
# Find key paragraphs
p_lab_text = None
p_lab_caption = None
p_web_text = None
p_web_caption = None
p_recommended = None
p_tc3 = None

for p in doc.paragraphs:
    txt = p.text.strip()
    if "Figure 5 demonstrates prototype testing" in txt:
        p_lab_text = p
    elif txt == "Fig. 5. Prototype testing under simulated solar illumination.":
        p_lab_caption = p
    elif "Figure 6 presents the user interface of the Web Dashboard" in txt:
        p_web_text = p
    elif txt == "Fig. 6. Cloud Web Dashboard hosted on Firebase Hosting displaying real-time telemetry and control panels.":
        p_web_caption = p
    elif "RECOMMENDED PHOTOGRAPHS FOR FINAL REPORT SUBMISSION" in txt:
        p_recommended = p
    elif txt.startswith("Test Case 3 — Energy Harvesting Efficiency Comparison"):
        p_tc3 = p

assert p_lab_text is not None, "p_lab_text not found"
assert p_lab_caption is not None, "p_lab_caption not found"
assert p_web_text is not None, "p_web_text not found"
assert p_web_caption is not None, "p_web_caption not found"
assert p_recommended is not None, "p_recommended not found"
assert p_tc3 is not None, "p_tc3 not found"

def add_body_p(target_before, text):
    p = target_before.insert_paragraph_before()
    p.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
    r = p.add_run(text)
    r.font.name = "Times New Roman"
    r.font.size = Pt(10)
    return p

def add_image_p(target_before, img_path, width):
    p = target_before.insert_paragraph_before()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    r = p.add_run()
    r.add_picture(img_path, width=width)
    return p

def add_caption_p(target_before, caption_text):
    p = target_before.insert_paragraph_before()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    r = p.add_run(caption_text)
    r.font.name = "Times New Roman"
    r.font.size = Pt(10)
    r.font.italic = True
    return p

# Insert Fig. 5 (Sensor Head) before p_lab_text
img_sensor = os.path.join(img_dir, "890490310_2615161338930352_5025945153904215907_n.jpg")
add_body_p(p_lab_text, "To enable high-sensitivity differential sun tracking without requiring expensive optical encoders, the 4-quadrant LDR sensor head is constructed with an opaque cross-shaped shadow baffle mounted directly above the photovoltaic panel (Fig. 5). Four photoresistors (TL, TR, BL, BR) are isolated in separate quadrants. When sunlight impinges perpendicular to the sensor normal, all quadrants receive balanced illumination. As the solar vector deviates, the central partitions cast distinct shadows across opposing quadrants, creating significant analog differential voltages that allow the Arduino Uno to compute instantaneous horizontal and vertical error signals.")
add_image_p(p_lab_text, img_sensor, Inches(5.2))
add_caption_p(p_lab_text, "Fig. 5. Macro photograph of the 4-quadrant LDR sensor head with perpendicular cross-shadow partition.")

# Insert Fig. 6 (LCD 2004 Display) before p_lab_text
img_lcd = os.path.join(img_dir, "896731412_1982675539374729_3434448632601237214_n.jpg")
add_body_p(p_lab_text, "The system incorporates an autonomous local diagnostic interface utilizing a 20x4 alphanumeric I2C character LCD (Fig. 6). The display provides immediate field feedback without requiring an internet connection: Row 1 confirms group identity ('NHOM 1 - SE2044'); Row 2 reports the ESP32-S3 link and Wi-Fi connectivity state; Row 3 displays the 18650 lithium battery state of charge (percentage and cell terminal voltage); and Row 4 shows real-time solar panel bus voltage and charging relay status ('R:ON' during active charging or 'R:OFF' when isolated).")
add_image_p(p_lab_text, img_lcd, Inches(3.8))
add_caption_p(p_lab_text, "Fig. 6. Local 20x4 I2C LCD displaying live system telemetry, group identification (\"NHOM 1 - SE2044\"), battery state, and charging relay status.")

# Update p_lab_text and p_lab_caption to be Fig. 7
p_lab_text.text = ""
p_lab_text.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
r_lab_t = p_lab_text.add_run("Initial tracking functionality was validated in the laboratory using simulated directional light beams (Fig. 7). As the beam moved across horizontal and vertical trajectories, the tracking algorithm generated differential error signals that drove the servos smoothly until light balance was restored, while the local LCD updated live metrics in real time.")
r_lab_t.font.name = "Times New Roman"
r_lab_t.font.size = Pt(10)

p_lab_caption.text = ""
p_lab_caption.alignment = WD_ALIGN_PARAGRAPH.CENTER
r_lab_c = p_lab_caption.add_run("Fig. 7. Laboratory prototype tracking validation under simulated directional illumination.")
r_lab_c.font.name = "Times New Roman"
r_lab_c.font.size = Pt(10)
r_lab_c.font.italic = True

# Insert Fig. 8 (Outdoor Deployment) before p_web_text
img_outdoor = os.path.join(img_dir, "892916864_1437335821871126_2798165777591257525_n.jpg")
add_body_p(p_web_text, "Following laboratory calibration, the integrated prototype was deployed in an outdoor environment on a building balcony ledge to evaluate tracking performance and power generation under real-world solar irradiation (Fig. 8). Under ambient outdoor conditions, the tracker stably followed the solar trajectory across the sky throughout daylight hours, withstanding gentle wind currents and maximizing photovoltaic power transferred into the 18650 lithium battery.")
add_image_p(p_web_text, img_outdoor, Inches(5.2))
add_caption_p(p_web_text, "Fig. 8. Experimental prototype deployed outdoors under natural sunlight for live solar tracking and energy harvesting evaluation.")

# Update p_web_text and p_web_caption to be Fig. 9
p_web_text.text = ""
p_web_text.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
r_web_t = p_web_text.add_run("Figure 9 presents the user interface of the Web Dashboard deployed on Firebase Hosting (https://solarcontroller-1cc93.web.app). Telemetry cards render real-time solar generation (voltage, current, power), cumulative energy in Wh, battery percentage, and 4-quadrant LDR values. Operators can toggle Auto/Manual modes, manipulate pan-tilt sliders, configure voltage thresholds, and execute emergency stops.")
r_web_t.font.name = "Times New Roman"
r_web_t.font.size = Pt(10)

p_web_caption.text = ""
p_web_caption.alignment = WD_ALIGN_PARAGRAPH.CENTER
r_web_c = p_web_caption.add_run("Fig. 9. Cloud Web Dashboard hosted on Firebase Hosting displaying real-time telemetry, energy accumulation, and system controls.")
r_web_c.font.name = "Times New Roman"
r_web_c.font.size = Pt(10)
r_web_c.font.italic = True

# Delete p_recommended
p_recommended._p.getparent().remove(p_recommended._p)

# Update p_tc3 (Test Case 3) to reference Fig. 8
p_tc3.text = ""
p_tc3.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
r_tc3 = p_tc3.add_run("Test Case 3 — Energy Harvesting Efficiency Comparison: A side-by-side comparative experiment was conducted over an 8-hour testing window (from 08:00 to 16:00) comparing a stationary solar panel fixed at a 15° south tilt with the developed dual-axis solar tracker under identical ambient conditions (as shown in the outdoor deployment in Fig. 8). Electrical output was sampled hourly using calibrated INA219 sensors. The empirical results are detailed in Table IV.")
r_tc3.font.name = "Times New Roman"
r_tc3.font.size = Pt(10)

# Save target docx
doc.save(target_docx)
print(f"Successfully generated finalized report: {target_docx}")
