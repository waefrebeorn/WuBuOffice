#!/usr/bin/env python3
"""Validate WuBuOffice-written ODF files with the INDEPENDENT odfpy library, and
also PRODUCE odfpy files for WuBuOffice to read back (control of destiny both
ways). Usage: odf_oracle.py {validate|produce} <dir>

validate <dir>: read <dir>/wubu.odt/.ods/.odp (written by the C test) and print
                extracted content; exit 0 if all parse, 1 on mismatch.
produce  <dir>: write <dir>/foreign.odt/.ods/.odp with odfpy for the C reader.

Exit 2 if odfpy is unavailable (caller SKIPs)."""
import sys, os

def main():
    if len(sys.argv) < 3:
        print("usage: odf_oracle.py {validate|produce} <dir>"); return 2
    mode, d = sys.argv[1], sys.argv[2]
    try:
        from odf.opendocument import OpenDocumentText, OpenDocumentSpreadsheet, OpenDocumentPresentation, load
        from odf.text import P, H
        from odf.table import Table, TableRow, TableCell
        from odf.draw import Page, Frame, TextBox
        from odf.style import MasterPage, PageLayout
        from odf.teletype import extractText
    except Exception:
        print("odfpy unavailable"); return 2

    os.makedirs(d, exist_ok=True)

    if mode == "produce":
        # .odt
        t = OpenDocumentText()
        t.text.addElement(H(outlinelevel=1, text="Foreign Heading"))
        t.text.addElement(P(text="Foreign paragraph."))
        t.save(os.path.join(d, "foreign.odt"))
        # .ods
        s = OpenDocumentSpreadsheet()
        tbl = Table(name="ForeignSheet")
        row = TableRow()
        c1 = TableCell(valuetype="string"); c1.addElement(P(text="Name")); row.addElement(c1)
        c2 = TableCell(valuetype="float", value="99.5"); c2.addElement(P(text="99.5")); row.addElement(c2)
        tbl.addElement(row)
        s.spreadsheet.addElement(tbl)
        s.save(os.path.join(d, "foreign.ods"))
        # .odp (odfpy's presentation API is finicky; best-effort)
        try:
            pr = OpenDocumentPresentation()
            pl = PageLayout(name="PL0")
            pr.automaticstyles.addElement(pl)
            mp = MasterPage(name="Standard", pagelayoutname="PL0")
            pr.masterstyles.addElement(mp)
            page = Page(name="p1", masterpagename="Standard")
            fr = Frame(width="20cm", height="3cm", x="2cm", y="1cm")
            fr.setAttrNS("urn:oasis:names:tc:opendocument:xmlns:presentation:1.0", "class", "title")
            tb = TextBox(); tb.addElement(P(text="Foreign Slide")); fr.addElement(tb); page.addElement(fr)
            pr.presentation.addElement(page)
            pr.save(os.path.join(d, "foreign.odp"))
            print("produced foreign.odt/.ods/.odp")
        except Exception as e:
            print("produced foreign.odt/.ods (odp skipped:", e, ")")
        return 0

    if mode == "validate":
        ok = True
        # odt
        doc = load(os.path.join(d, "wubu.odt"))
        heads = [extractText(h) for h in doc.getElementsByType(H)]
        if "ODF Title" not in heads: print("odt heading missing", heads); ok = False
        # ods
        doc = load(os.path.join(d, "wubu.ods"))
        tables = doc.getElementsByType(Table)
        if len(tables) < 2: print("ods sheets", len(tables)); ok = False
        # odp
        doc = load(os.path.join(d, "wubu.odp"))
        ps = [extractText(p) for p in doc.getElementsByType(P)]
        if "Slide One" not in ps: print("odp title missing", ps); ok = False
        print("odfpy validation", "OK" if ok else "FAILED")
        return 0 if ok else 1

    print("unknown mode"); return 2

if __name__ == "__main__":
    sys.exit(main())
