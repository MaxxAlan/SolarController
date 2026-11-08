from pypdf import PdfReader

reader = PdfReader(r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Document\Slides\IOT - Project.pdf")
print("Num pages:", len(reader.pages))
for i in range(min(5, len(reader.pages))):
    print(f"--- SLIDE {i+1} ---")
    print(reader.pages[i].extract_text()[:400])
