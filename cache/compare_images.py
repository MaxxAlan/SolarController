import zipfile
import hashlib
import os

docx_path = r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Document\Report\Group1_SolarTracker_Report.docx"
img_dir = r"D:\project\SolarController\img"

def get_hash(data):
    return hashlib.md5(data).hexdigest()

print("=== DOCX MEDIA ===")
with zipfile.ZipFile(docx_path) as z:
    for name in z.namelist():
        if name.startswith("word/media/"):
            data = z.read(name)
            h = get_hash(data)
            print(f"{name}: {len(data)} bytes, md5={h}")

print("\n=== IMG DIRECTORY ===")
for fname in os.listdir(img_dir):
    fpath = os.path.join(img_dir, fname)
    if os.path.isfile(fpath):
        with open(fpath, "rb") as f:
            data = f.read()
        h = get_hash(data)
        print(f"{fname}: {len(data)} bytes, md5={h}")
