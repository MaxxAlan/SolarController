import docx

doc_path = r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Document\Report\Group1_SolarTracker_Report.docx"
doc = docx.Document(doc_path)

print(f"Number of tables: {len(doc.tables)}")
for idx, table in enumerate(doc.tables):
    print(f"\n==================== TABLE {idx + 1} ====================")
    for row in table.rows:
        row_txt = [cell.text.strip().replace("\n", " ") for cell in row.cells]
        print(" | ".join(row_txt))
