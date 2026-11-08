import docx
import re

doc_path = r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Document\Report\Group1_SolarTracker_Report.docx"
doc = docx.Document(doc_path)

print("Mentions of Fig or Figure in paragraphs:")
for i, p in enumerate(doc.paragraphs):
    txt = p.text
    matches = re.findall(r'(Fig(?:ure)?\.?\s*\d+)', txt, re.IGNORECASE)
    if matches:
        print(f"P{i:03d}: {matches} -> {txt[:100]}")
