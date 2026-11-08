import openpyxl

wb_path = r"D:\project\SolarController\SE2044_G1_IOT102t_FinalReport\Contribution\Contribution.xlsx"
wb = openpyxl.load_workbook(wb_path)
for sheet_name in wb.sheetnames:
    ws = wb[sheet_name]
    print(f"=== Sheet: {sheet_name} ===")
    for row in ws.iter_rows(values_only=True):
        if any(row):
            print(row)
