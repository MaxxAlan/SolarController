import docx

doc_path = r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Document\Report\Group1_SolarTracker_Report.docx"
doc = docx.Document(doc_path)

for i, p in enumerate(doc.paragraphs):
    xml = p._p.xml
    if "drawing" in xml:
        # find extent
        import xml.etree.ElementTree as ET
        tree = ET.fromstring(xml)
        ns = {
            'wp': 'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing',
            'a': 'http://schemas.openxmlformats.org/drawingml/2006/main',
            'r': 'http://schemas.openxmlformats.org/officeDocument/2006/relationships'
        }
        extents = tree.findall('.//wp:extent', ns)
        blips = tree.findall('.//a:blip', ns)
        for ext, blip in zip(extents, blips):
            cx = int(ext.attrib.get('cx', 0))
            cy = int(ext.attrib.get('cy', 0))
            rid = blip.attrib.get('{http://schemas.openxmlformats.org/officeDocument/2006/relationships}embed')
            target = doc.part.rels[rid].target_ref if rid in doc.part.rels else 'unknown'
            caption = doc.paragraphs[i+1].text[:60] if i+1 < len(doc.paragraphs) else ''
            print(f"P{i:03d}: {target} -> width={cx/914400:.2f} in ({cx} EMU), height={cy/914400:.2f} in ({cy} EMU) | Next: {caption}")
