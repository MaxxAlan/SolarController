import docx

doc = docx.Document(r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Document\Report\Group1_SolarTracker_Report_Final.docx")
start_idx = None
for i, p in enumerate(doc.paragraphs):
    if "III. RESULTS AND DISCUSSION" in p.text:
        start_idx = i
        break

print(f"Start idx: {start_idx}")
for i in range(start_idx, min(start_idx + 25, len(doc.paragraphs))):
    p = doc.paragraphs[i]
    print(f"P{i:03d} [{p.style.name}]: {p.text[:80]}")
