#include "wubudoc_internal.h"

/* ---------- kind detection from a path/type ---------- */
static const char *kind_of_ext(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return NULL;
    const char *e = dot + 1;
    if (!strcasecmp(e, "txt")) return "txt";
    if (!strcasecmp(e, "md"))  return "md";
    if (!strcasecmp(e, "json")) return "json";
    if (!strcasecmp(e, "csv")) return "csv";
    if (!strcasecmp(e, "tsv")) return "tsv";
    if (!strcasecmp(e, "svg")) return "svg";
    if (!strcasecmp(e, "xml")) return "xml";
    if (!strcasecmp(e, "html") || !strcasecmp(e, "htm")) return "html";
    if (!strcasecmp(e, "rtf")) return "rtf";
    if (!strcasecmp(e, "epub")) return "epub";
    if (!strcasecmp(e, "ttf") || !strcasecmp(e, "otf")) return "font";
    if (!strcasecmp(e, "woff")) return "woff";
    if (!strcasecmp(e, "zip")) return "zip";
    if (!strcasecmp(e, "docx")) return "docx";
    if (!strcasecmp(e, "xlsx")) return "xlsx";
    if (!strcasecmp(e, "pptx")) return "pptx";
    if (!strcasecmp(e, "odt"))  return "odt";
    if (!strcasecmp(e, "ods"))  return "ods";
    if (!strcasecmp(e, "odp"))  return "odp";
    if (!strcasecmp(e, "fodt")) return "fodt";
    if (!strcasecmp(e, "fods")) return "fods";
    if (!strcasecmp(e, "fodp")) return "fodp";
    if (!strcasecmp(e, "doc")) return "doc";
    if (!strcasecmp(e, "xls")) return "xls";
    if (!strcasecmp(e, "ppt")) return "ppt";
    return NULL;
}
static const char *sniff_kind(const uint8_t *d, size_t len) {
    if (!d || len == 0) return NULL;
    /* PDF */
    if (len >= 4 && memcmp(d, "%PDF", 4) == 0) return "pdf";
    /* PNG */
    static const uint8_t pngsig[8] = {0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A};
    if (len >= 8 && memcmp(d, pngsig, 8) == 0) return "png";
    /* GIF */
    if (len >= 4 && memcmp(d, "GIF8", 4) == 0) return "gif";
    /* JPEG */
    if (len >= 3 && d[0]==0xFF && d[1]==0xD8 && d[2]==0xFF) return "jpg";
    /* ZIP family (docx/xlsx/pptx/odt/ods/odp/zip) */
    if (len >= 4 && memcmp(d, "PK\x03\x04", 4) == 0) return "zip";
    /* CFB (legacy doc/xls/ppt) */
    static const uint8_t cfb[8] = {0xD0,0xCF,0x11,0xE0,0xA1,0xB1,0x1A,0xE1};
    if (len >= 8 && memcmp(d, cfb, 8) == 0) return "doc";
    /* PostScript */
    if (len >= 2 && d[0]=='%' && d[1]=='!') return "ps";
    /* Text-ish: every byte is printable/whitespace (no NUL, no high bytes). */
    int istext = 1;
    for (size_t i = 0; i < len; i++) {
        uint8_t c = d[i];
        if (c=='\n' || c=='\r' || c=='\t' || (c>=0x20 && c<=0x7E)) continue;
        istext = 0; break;
    }
    if (istext) {
        if (len >= 1 && d[0]=='{') return "json";
        if (len >= 5 && (memcmp(d, "<?xml", 5)==0 || memcmp(d, "<svg", 4)==0
                         || memcmp(d, "<!DO", 4)==0)) return "xml";
        if (len >= 1 && d[0]=='<') return "html";   /* some other XML/HTML-ish */
        return "txt";
    }
    return NULL;
}

/* Peek inside a ZIP to choose the right office kind (OOXML vs ODF) so text
 * extraction routes through the right wubuconv path. Returns a static kind. */
static const char *classify_zip(const uint8_t *d, size_t len) {
    wubuzip_archive z;
    if (wubuzip_open(d, len, &z) != 0) return "zip";
    const char *kind = "zip";
    for (size_t i = 0; i < wubuzip_count(&z); i++) {
        const char *nm = wubuzip_name(&z, i);
        if (!nm) continue;
        if (strncmp(nm, "word/", 5) == 0)      { kind = "docx"; break; }
        if (strncmp(nm, "xl/", 3) == 0)        { kind = "xlsx"; break; }
        if (strncmp(nm, "ppt/", 4) == 0)       { kind = "pptx"; break; }
        if (strncmp(nm, "META-INF/", 9) == 0
            || strcmp(nm, "content.xml") == 0
            || strcmp(nm, "styles.xml") == 0)  { kind = "odt";  } /* refined below */
    }
    /* Distinguish ODF sub-types by the (stored, first) mimetype entry. */
    if (strcmp(kind, "odt") == 0) {
        for (size_t i = 0; i < wubuzip_count(&z); i++) {
            const char *nm = wubuzip_name(&z, i);
            if (nm && strcmp(nm, "mimetype") == 0) {
                uint8_t *b = NULL; size_t bl = 0;
                wubuzip_extract(&z, i, &b, &bl);
                if (b) {
                    if (memcmp(b, "application/vnd.oasis.opendocument.spreadsheet", 38)==0) kind="ods";
                    else if (memcmp(b, "application/vnd.oasis.opendocument.presentation", 43)==0) kind="odp";
                    else if (memcmp(b, "application/vnd.oasis.opendocument.text", 34)==0) kind="odt";
                    free(b);
                }
                break;
            }
        }
    }
    wubuzip_close(&z);
    return kind;
}
