import docx

doc_path = r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Document\Report\Group1_SolarTracker_Report.docx"
doc = docx.Document(doc_path)

print(f"Number of sections: {len(doc.sections)}")
for i, sec in enumerate(doc.sections):
    print(f"Section {i}: start_type={sec.start_type}, orientation={sec.orientation}, page_width={sec.page_width}, page_height={sec.page_height}")
    # check columns
    sectPr = sec._sectPr
    cols = sectPr.xpath("./w:cols")
    if cols:
        print(f"  w:cols attributes: {cols[0].attrib}")
    else:
        print("  w:cols: default (1 col)")

print("\nSample styles:")
for p in doc.paragraphs[:15]:
    runs_info = [(r.text[:20], r.font.name, r.font.size, r.font.bold, r.font.italic) for r in p.runs[:3]]
    print(f"P: {p.text[:40]} | Style: {p.style.name} | Runs: {runs_info}")
