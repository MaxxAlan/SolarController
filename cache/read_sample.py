from pypdf import PdfReader

reader = PdfReader(r"D:\project\SolarController\IoT_Project_Report_Examples.pdf")
print("Num pages:", len(reader.pages))
for i in range(min(5, len(reader.pages))):
    print(f"--- PAGE {i+1} ---")
    print(reader.pages[i].extract_text()[:1200])
