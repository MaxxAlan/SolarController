import docx
import re
import os

doc_path = r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Document\Report\Group1_SolarTracker_Report.docx"
doc = docx.Document(doc_path)

print(f"Total paragraphs: {len(doc.paragraphs)}")
for i, p in enumerate(doc.paragraphs):
    xml = p._p.xml
    if "blip" in xml:
        r_ids = re.findall(r'r:embed="([^"]+)"', xml)
        for rid in r_ids:
            target = doc.part.rels[rid].target_ref
            caption = doc.paragraphs[i+1].text.strip() if i+1 < len(doc.paragraphs) else ""
            print(f"P[{i}] -> rId={rid} -> {target} | Caption: {caption}")
