import docx

doc_path = r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Document\Report\Group1_SolarTracker_Report.docx"
doc = docx.Document(doc_path)

print("Checking for placeholders (★, TODO, NOTE, RECOMMENDED, FIXME)...")
for i, p in enumerate(doc.paragraphs):
    txt = p.text
    for kw in ["★", "TODO", "FIXME", "RECOMMENDED", "[ ?]", "xxx", "XXX"]:
        if kw in txt:
            print(f"P[{i}]: ({kw}) -> {txt[:120]}")
