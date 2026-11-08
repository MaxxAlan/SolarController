from pypdf import PdfReader
import os

def read_pdf(path):
    print(f"=== {os.path.basename(path)} ===")
    reader = PdfReader(path)
    print(f"Num pages: {len(reader.pages)}")
    for i, page in enumerate(reader.pages):
        txt = page.extract_text()
        print(f"--- PAGE {i+1} ---")
        print(txt[:1000])

read_pdf(r"D:\project\SolarController\Class_Group_IOT102_FinalProjectReport.pdf")
