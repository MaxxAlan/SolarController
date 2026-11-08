import docx

doc_path = r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Document\Report\Group1_SolarTracker_Report.docx"
doc = docx.Document(doc_path)

print(f"=== FULL AUDIT OF PARAGRAPHS ({len(doc.paragraphs)}) ===")
for i, p in enumerate(doc.paragraphs):
    txt = p.text.strip()
    if not txt:
        continue
    # Check if heading or caption or special
    prefix = ""
    if p.style.name.startswith("Heading"):
        prefix = f"[{p.style.name}] "
    print(f"P{i:03d}: {prefix}{txt}")
