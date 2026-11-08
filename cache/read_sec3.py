import docx

doc_path = r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Document\Report\Group1_SolarTracker_Report.docx"
doc = docx.Document(doc_path)

for i in range(67, 82):
    p = doc.paragraphs[i]
    print(f"P{i:03d} [{p.style.name}]: {p.text}")
