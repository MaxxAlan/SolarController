import docx
import re

doc_path = r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Document\Report\Group1_SolarTracker_Report.docx"
doc = docx.Document(doc_path)

print("Checking 'tilt' or '160' or 'angle' in docx paragraphs...")
for i, p in enumerate(doc.paragraphs):
    txt = p.text
    if "160" in txt or "tilt" in txt.lower():
        print(f"P[{i}]: {txt}")
