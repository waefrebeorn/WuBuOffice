/* table_styles.h -- H6: DOCX table style resolution.
 *
 * Word formats tables through a style CHAIN: w:tblStyle names a table style,
 * the style has w:basedOn parents, and conditional-format overrides
 * (firstRow, lastRow, band1H/band2H ...) are switched on by w:tblLook flags
 * plus per-cell w:cnfStyle. Resolving the chain -- walking basedOn to the
 * root and merging each level's properties, later (more derived) winning --
 * is what makes a header row come out bold and centered without any direct
 * formatting in the file. */
#ifndef WUBUOXML_TABLE_STYLES_H
#define WUBUOXML_TABLE_STYLES_H

#include <stddef.h>

typedef struct {
    int  bold;          /* run bold on/off (-1 inherit sentinel unused after resolve) */
    int  centered;      /* paragraph justification center */
    int  shading_r, shading_g, shading_b;  /* cell fill, -1 = none */
} TblCellProps;

typedef struct {
    char     id[64];        /* w:styleId */
    char     based_on[64];  /* w:basedOn val, or "" */
    /* unconditional props */
    int      bold, centered;
    int      shading_r, shading_g, shading_b;
    /* conditional props (per flag) */
    TblCellProps first_row, last_row;
    TblCellProps band1h, band2h;   /* even/odd banded rows */
    int has_first_row, has_last_row, has_band1h, has_band2h;
} TableStyle;

/* tblLook bit flags from OOXML spec */
#define TS_LOOK_FIRST_ROW    0x0020u
#define TS_LOOK_LAST_ROW     0x0040u
#define TS_LOOK_FIRST_COL    0x0080u
#define TS_LOOK_LAST_COL     0x0100u
#define TS_LOOK_NO_HBAND     0x0200u
#define TS_LOOK_NO_VBAND     0x0400u

/* Parse one styles.xml document into an array of table styles.
 * Returns malloc'd array (caller frees each id/based_on via free_table_styles)
 * or NULL; *count set. Only w:type="table" styles are collected. */
TableStyle *table_styles_parse(const char *styles_xml, size_t len, size_t *count);

/* Free a styles array returned by table_styles_parse. */
void table_styles_free(TableStyle *arr);

/* Resolve: walk the basedOn chain root->derived, merging props (derived
 * wins), then apply the tblLook-selected conditional format for a given
 * row index (row 0 = first). Returns resolved cell props. */
TblCellProps table_styles_resolve(const TableStyle *styles, size_t count,
                                  const char *style_id, unsigned look_flags,
                                  int row_index);

#endif /* WUBUOXML_TABLE_STYLES_H */
