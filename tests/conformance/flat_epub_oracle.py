#!/usr/bin/env python3
"""Independent oracle for flat-ODF (.fodt/.fods/.fodp) and EPUB (.epub).

Modes:
  check_epub <path> <kw1> <kw2> ...
      Read the EPUB with EbookLib (an independent, third-party EPUB library).
      Verify it parses, has >=1 spine item and >=1 navigation entry, and that
      every keyword appears in the concatenated chapter text. exit 0 pass,
      1 fail, 2 if EbookLib is unavailable (test SKIPs).

  check_flat <path> <expect_root_child> <kw1> <kw2> ...
      Validate a flat-ODF single-XML file with the stdlib XML parser: it must be
      well-formed, have <office:document> as the root with an office:mimetype
      attribute, contain the expected inner root (office:text/spreadsheet/
      presentation), and include every keyword in its text nodes. Uses only the
      Python standard library, so it never SKIPs. exit 0 pass, 1 fail.
"""
import sys
import xml.dom.minidom as minidom


def _all_text(node, out):
    if node.nodeType == node.TEXT_NODE:
        out.append(node.data)
    for c in node.childNodes:
        _all_text(c, out)


def check_flat(path, expect_child, keywords):
    try:
        doc = minidom.parse(path)
    except Exception as e:
        print("flat XML parse error:", e)
        return 1
    root = doc.documentElement
    if root.tagName != "office:document":
        print("bad root:", root.tagName)
        return 1
    if not root.getAttribute("office:mimetype"):
        print("missing office:mimetype")
        return 1
    if len(doc.getElementsByTagName(expect_child)) != 1:
        print("missing inner root:", expect_child)
        return 1
    chunks = []
    _all_text(root, chunks)
    text = " ".join(chunks)
    for kw in keywords:
        if kw not in text:
            print("MISSING keyword:", repr(kw))
            return 1
    print("flat OK root=office:document child=%s" % expect_child)
    return 0


def check_epub(path, keywords):
    try:
        import ebooklib
        from ebooklib import epub
    except ImportError:
        return 2
    try:
        book = epub.read_epub(path)
    except Exception as e:
        print("epub read error:", e)
        return 1
    docs = [it for it in book.get_items() if it.get_type() == ebooklib.ITEM_DOCUMENT]
    if len(book.spine) < 1:
        print("empty spine")
        return 1
    if len(book.toc) < 1:
        print("empty toc")
        return 1
    import re
    text = ""
    for d in docs:
        text += re.sub(r"<[^>]+>", " ", d.get_content().decode("utf-8", "replace"))
    for kw in keywords:
        if kw not in text:
            print("MISSING keyword:", repr(kw))
            return 1
    print("epub OK spine=%d toc=%d docs=%d" % (len(book.spine), len(book.toc), len(docs)))
    return 0


def main():
    if len(sys.argv) < 2:
        return 1
    mode = sys.argv[1]
    if mode == "check_flat":
        return check_flat(sys.argv[2], sys.argv[3], sys.argv[4:])
    if mode == "check_epub":
        return check_epub(sys.argv[2], sys.argv[3:])
    return 1


if __name__ == "__main__":
    sys.exit(main())
