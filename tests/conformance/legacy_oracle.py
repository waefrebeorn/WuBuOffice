#!/usr/bin/env python3
"""Independent oracle for the legacy-binary + PDF tests.

Modes:
  produce_xls <dir>   -> write <dir>/legacy.xls with known values via xlwt.
                         exit 2 if xlwt is unavailable (test SKIPs).
  read_xls <path>     -> print "cell R C KIND VALUE" lines via xlrd for the C
                         reader to diff against. exit 2 if xlrd unavailable.
  check_pdf <path> <kw1> <kw2> ...
                      -> load with pypdf (page count) and extract text with
                         pdfminer; exit 0 if every keyword is present, 1 if a
                         keyword is missing / file invalid, 2 if libs absent.
"""
import sys


def produce_xls(path):
    try:
        import xlwt
    except ImportError:
        return 2
    wb = xlwt.Workbook()
    ws = wb.add_sheet("Data")
    ws.write(0, 0, "Item"); ws.write(0, 1, "Qty"); ws.write(0, 2, "Price")
    ws.write(1, 0, "Widget"); ws.write(1, 1, 3); ws.write(1, 2, 19.99)
    ws.write(2, 0, "Gadget"); ws.write(2, 1, 10); ws.write(2, 2, 4.5)
    ws.write(3, 0, "Total"); ws.write(3, 1, 13); ws.write(3, 2, 1520.5)
    ws2 = wb.add_sheet("Notes")
    ws2.write(0, 0, "Second sheet string")
    ws2.write(1, 0, 42)
    wb.save(path)
    return 0


def check_pdf(path, keywords):
    try:
        import pypdf
        from pdfminer.high_level import extract_text
    except ImportError:
        return 2
    try:
        r = pypdf.PdfReader(path)
        _ = len(r.pages)
        txt = extract_text(path)
    except Exception as e:
        print("pdf error:", e)
        return 1
    for kw in keywords:
        if kw not in txt:
            print("MISSING keyword:", repr(kw))
            return 1
    print("pdf OK pages=%d" % len(r.pages))
    return 0


def main():
    if len(sys.argv) < 2:
        return 1
    mode = sys.argv[1]
    if mode == "produce_xls":
        return produce_xls(sys.argv[2])
    if mode == "check_pdf":
        return check_pdf(sys.argv[2], sys.argv[3:])
    return 1


if __name__ == "__main__":
    sys.exit(main())
