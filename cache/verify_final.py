import docx
import zipfile
import re

doc_path = r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Document\Report\Group1_SolarTracker_Report_Final.docx"
doc = docx.Document(doc_path)

print(f"Total paragraphs: {len(doc.paragraphs)}")
print(f"Total tables: {len(doc.tables)}")

print("\n=== VERIFY HEADER ===")
for i in range(1, 9):
    print(f"P{i:02d}: {doc.paragraphs[i].text}")

print("\n=== VERIFY ALL FIGURE CAPTIONS AND REFERENCES ===")
for i, p in enumerate(doc.paragraphs):
    txt = p.text.strip()
    if re.search(r'\bFig(?:ure)?\.?\s*\d+', txt, re.IGNORECASE):
        print(f"P{i:03d}: {txt[:100]}")

print("\n=== VERIFY IMAGES IN ZIP ===")
with zipfile.ZipFile(doc_path) as z:
    for name in z.namelist():
        if name.startswith("word/media/"):
            info = z.getinfo(name)
            print(f"  {name}: {info.file_size} bytes")

print("\n=== VERIFY ANY REMAINING PLACEHOLDERS ===")
found_ph = False
for i, p in enumerate(doc.paragraphs):
    for ph in ["★", "TODO", "RECOMMENDED", "[ ?]", "FIXME"]:
        if ph in p.text:
            print(f"Found placeholder {ph} in P{i}: {p.text[:80]}")
            found_ph = True
if not found_ph:
    print("NO placeholders found! Clean!")
