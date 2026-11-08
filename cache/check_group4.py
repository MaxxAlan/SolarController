import docx
import re

doc_path = r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Document\Report\Group1_SolarTracker_Report.docx"
doc = docx.Document(doc_path)

print("Checking text for 'group' or 'nhom' or '4'...")
for i, p in enumerate(doc.paragraphs):
    txt = p.text
    if re.search(r'\b(gr|group|nhom|nhóm)\s*4\b', txt, re.IGNORECASE):
        print(f"P[{i}]: {txt}")
    if "SE2044" in txt:
        print(f"SE2044 in P[{i}]: {txt}")

for t_idx, table in enumerate(doc.tables):
    for r_idx, row in enumerate(table.rows):
        for c_idx, cell in enumerate(row.cells):
            txt = cell.text
            if re.search(r'\b(gr|group|nhom|nhóm)\s*4\b', txt, re.IGNORECASE):
                print(f"Table[{t_idx}] R{r_idx}C{c_idx}: {txt}")
