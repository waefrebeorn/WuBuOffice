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


def check_xls(path, keywords):
    """Validate a .xls we wrote: open with xlrd, require every keyword to appear
    as a sheet name or cell value. exit 2 if xlrd absent."""
    try:
        import xlrd
    except ImportError:
        return 2
    try:
        b = xlrd.open_workbook(path)
    except Exception as e:
        print("xls error:", e)
        return 1
    found = []
    for sh in b.sheets():
        found.append(sh.name)
        for rr in range(sh.nrows):
            for cc in range(sh.ncols):
                v = sh.cell_value(rr, cc)
                if isinstance(v, (int, float)):
                    found.append(str(v))
                else:
                    found.append(str(v))
    blob = "\n".join(found)
    for kw in keywords:
        if kw not in blob:
            print("MISSING keyword:", repr(kw))
            return 1
    print("xls OK sheets=%d" % len(b.sheets()))
    return 0


def _doc_text(path):
    """Independent FIB/CLX parse of a .doc -> recovered text (mimics Word)."""
    import olefile, struct
    o = olefile.OleFileIO(path)
    wd = o.openstream("WordDocument").read()
    if wd[0] != 0xEC or wd[1] != 0xA5:
        raise ValueError("bad wIdent")
    nFib = struct.unpack("<H", wd[2:4])[0]
    which = struct.unpack("<H", wd[0x0A:0x0C])[0] & 0x0200
    fcClx = struct.unpack("<I", wd[0x1A2:0x1A6])[0]
    lcbClx = struct.unpack("<I", wd[0x1A6:0x1AA])[0]
    tname = "1Table" if which else "0Table"
    tb = o.openstream(tname).read()
    clxt = tb[fcClx]
    if clxt != 0x02:
        raise ValueError("expected Pcdt clxt")
    lcb = struct.unpack("<I", tb[fcClx + 1:fcClx + 5])[0]
    plc = tb[fcClx + 5:fcClx + 5 + lcb]
    n = (lcb - 4) // 12
    cps = [struct.unpack("<I", plc[i * 4:i * 4 + 4])[0] for i in range(n + 1)]
    out = []
    for i in range(n):
        pcd = plc[(n + 1) * 4 + i * 8:(n + 1) * 4 + i * 8 + 8]
        fc = struct.unpack("<I", pcd[2:6])[0]
        comp = (fc & 0x40000000) != 0
        off = (fc & 0x3FFFFFFF) // 2 if comp else (fc & 0x3FFFFFFF)
        nch = cps[i + 1] - cps[i]
        if comp:
            out.append(wd[off:off + nch].decode("cp1252", "replace"))
        else:
            out.append(wd[off:off + nch * 2].decode("utf-16-le", "replace"))
    return "".join(out)


def check_doc(path, keywords):
    try:
        import olefile  # noqa
    except ImportError:
        return 2
    try:
        txt = _doc_text(path)
    except Exception as e:
        print("doc error:", e)
        return 1
    for kw in keywords:
        if kw not in txt:
            print("MISSING keyword:", repr(kw))
            return 1
    print("doc OK chars=%d" % len(txt))
    return 0


def _ppt_text(path):
    """Independent PPT record-walk -> recovered text (mimics PowerPoint)."""
    import olefile, struct
    o = olefile.OleFileIO(path)
    doc = o.openstream("PowerPoint Document").read()
    out = []
    def walk(d, off, end, depth):
        p = off
        while p + 8 <= end:
            ver, typ = struct.unpack("<HH", d[p:p + 4])
            rlen = struct.unpack("<I", d[p + 4:p + 8])[0]
            body = p + 8
            if body + rlen > end:
                rlen = end - body
            isc = (ver & 0x0F) == 0x0F
            if typ in (0x0FA0, 0x0FA8):
                if typ == 0x0FA0:
                    out.append(d[body:body + rlen].decode("utf-16-le", "replace"))
                else:
                    out.append(d[body:body + rlen].decode("cp1252", "replace"))
            elif isc:
                walk(d, body, body + rlen, depth + 1)
            p = body + rlen
    walk(doc, 0, len(doc), 0)
    return "".join(out)


def check_ppt(path, keywords):
    try:
        import olefile  # noqa
    except ImportError:
        return 2
    try:
        txt = _ppt_text(path)
    except Exception as e:
        print("ppt error:", e)
        return 1
    for kw in keywords:
        if kw not in txt:
            print("MISSING keyword:", repr(kw))
            return 1
    print("ppt OK chars=%d" % len(txt))
    return 0


def main():
    if len(sys.argv) < 2:
        return 1
    mode = sys.argv[1]
    if mode == "produce_xls":
        return produce_xls(sys.argv[2])
    if mode == "check_pdf":
        return check_pdf(sys.argv[2], sys.argv[3:])
    if mode == "check_xls":
        return check_xls(sys.argv[2], sys.argv[3:])
    if mode == "check_doc":
        return check_doc(sys.argv[2], sys.argv[3:])
    if mode == "check_ppt":
        return check_ppt(sys.argv[2], sys.argv[3:])
    return 1


if __name__ == "__main__":
    sys.exit(main())
