import os
import zipfile
from PIL import Image

cache_dir = r"D:\project\SolarController\cache"
media_dir = os.path.join(cache_dir, "docx_media")
os.makedirs(media_dir, exist_ok=True)

docx_path = r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Document\Report\Group1_SolarTracker_Report.docx"
with zipfile.ZipFile(docx_path) as z:
    for name in z.namelist():
        if name.startswith("word/media/"):
            fname = os.path.basename(name)
            out_path = os.path.join(media_dir, fname)
            with open(out_path, "wb") as f:
                f.write(z.read(name))
            im = Image.open(out_path)
            print(f"DOCX {fname}: size={im.size}, format={im.format}, mode={im.mode}")

img_dir = r"D:\project\SolarController\img"
print("\nIMG folder images:")
for fname in os.listdir(img_dir):
    fpath = os.path.join(img_dir, fname)
    if fname.lower().endswith(('.jpg', '.png')):
        try:
            im = Image.open(fpath)
            print(f"IMG {fname}: size={im.size}, format={im.format}, mode={im.mode}")
        except Exception as e:
            print(f"IMG {fname}: {e}")
