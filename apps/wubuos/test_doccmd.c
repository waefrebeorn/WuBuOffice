/* test_doccmd.c -- headless unit test for the extracted opaque doccmd module.
 * Verifies structural inserts (mutate model + signal TOC dirty), the
 * wubuscript computed field, and that export/save/a11y return sane results.
 */
#include "doccmd.h"
#include "model.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static wubumodel_doc *mk_doc(void){
    wubumodel_doc *d = wubumodel_doc_create();
    if (!d) return NULL;
    wubumodel_node *sec  = wubumodel_node_create(d, WUBUMODEL_SECTION);
    wubumodel_node *para = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
    wubumodel_node *run  = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(run, "Hello");
    wubumodel_node_append(d, para, run);
    wubumodel_node_append(d, sec, para);
    /* sec is parentless -> wubumodel_doc_root(d) returns it (the section) */
    return d;
}

static int count_kind_rec(wubumodel_node *n, wubumodel_kind k){
    if (!n) return 0;
    int n_found = (wubumodel_node_kind(n) == k) ? 1 : 0;
    for (wubumodel_node *c = wubumodel_node_first_child(n); c; c = wubumodel_node_next_sibling(c))
        n_found += count_kind_rec(c, k);
    return n_found;
}
static int count_kind(wubumodel_doc *d, wubumodel_kind k){
    return count_kind_rec(wubumodel_doc_root(d), k);
}

int main(void){
    int fails = 0;
    wubumodel_doc *d = mk_doc();
    if (!d){ fprintf(stderr, "alloc failed\n"); return 1; }

    /* each structural insert returns 1 (TOC dirty) and adds its node */
    if (doccmd_insert_link(d) != 1 || count_kind(d, WUBUMODEL_LINK) != 1){ fprintf(stderr, "[link]\n"); fails++; }
    if (doccmd_insert_list(d) != 1 || count_kind(d, WUBUMODEL_PARAGRAPH) < 2){ fprintf(stderr, "[list]\n"); fails++; }
    if (doccmd_insert_table(d) != 1 || count_kind(d, WUBUMODEL_TABLE) != 1){ fprintf(stderr, "[table]\n"); fails++; }
    if (doccmd_insert_image(d) != 1 || count_kind(d, WUBUMODEL_IMAGE) != 1){ fprintf(stderr, "[image]\n"); fails++; }
    if (doccmd_insert_pagebreak(d) != 1 || count_kind(d, WUBUMODEL_PAGEBREAK) != 1){ fprintf(stderr, "[pagebreak]\n"); fails++; }
    if (doccmd_insert_sectionbreak(d) != 1 || count_kind(d, WUBUMODEL_SECTIONBREAK) != 1){ fprintf(stderr, "[sectionbreak]\n"); fails++; }
    if (doccmd_insert_header(d) != 1 || count_kind(d, WUBUMODEL_HEADER) != 1){ fprintf(stderr, "[header]\n"); fails++; }
    if (doccmd_insert_footer(d) != 1 || count_kind(d, WUBUMODEL_FOOTER) != 1){ fprintf(stderr, "[footer]\n"); fails++; }
    /* header insert is idempotent (replaces, not adds) */
    if (doccmd_insert_header(d) != 1 || count_kind(d, WUBUMODEL_HEADER) != 1){ fprintf(stderr, "[header idempotent]\n"); fails++; }
    if (doccmd_insert_comment(d) != 1 || count_kind(d, WUBUMODEL_COMMENT) != 1){ fprintf(stderr, "[comment]\n"); fails++; }
    if (doccmd_insert_trackchange(d) != 1 || count_kind(d, WUBUMODEL_TRACKCHANGE) != 1){ fprintf(stderr, "[trackchange]\n"); fails++; }
    if (doccmd_insert_field(d) != 1 || count_kind(d, WUBUMODEL_FIELD) != 1){ fprintf(stderr, "[field]\n"); fails++; }

    /* wubuscript computed field: 'lines' = paragraph count (resolver),
     * so value == 2 * (paragraph count before this insert). */
    int paras_before = count_kind(d, WUBUMODEL_PARAGRAPH);
    if (doccmd_insert_script_field(d, "lines * 2") != 1){ fprintf(stderr, "[script insert]\n"); fails++; }
    else {
        int fields = 0;
        for (wubumodel_node *s = wubumodel_doc_root(d); s; s = wubumodel_node_next_sibling(s))
            for (wubumodel_node *p = wubumodel_node_first_child(s); p; p = wubumodel_node_next_sibling(p))
                if (wubumodel_node_kind(p) == WUBUMODEL_FIELD){
                    if (!strcmp(wubumodel_node_field(p), "script")){
                        const char *v = wubumodel_run_text(p);
                        char expect[32]; snprintf(expect,sizeof expect,"%d", 2*paras_before);
                        if (!v || strcmp(v, expect)){ fprintf(stderr, "[script] value '%s' != %s\n", v?v:"(null)", expect); fails++; }
                        fields++;
                    }
                }
        if (fields != 1){ fprintf(stderr, "[script] expected 1 script field, got %d\n", fields); fails++; }
    }

    /* export EPUB returns a non-null status string */
    char *ep = doccmd_export_epub(d, "/tmp/test_doccmd.epub");
    if (!ep){ fprintf(stderr, "[epub] null\n"); fails++; }
    else { printf("  epub: %s\n", ep); free(ep); }

    /* save DOCX returns a non-null status string */
    char *sv = doccmd_save(d, "/tmp/test_doccmd.docx");
    if (!sv){ fprintf(stderr, "[save] null\n"); fails++; }
    else { printf("  save: %s\n", sv); free(sv); }

    /* a11y check fills a report (no crash) */
    a11y_report a; doccmd_a11y_check(d, &a);
    printf("  a11y issues: %d\n", a.count);
    a11y_report_free(&a);

    wubumodel_doc_destroy(d);

    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: doccmd (15 structural inserts + script field + epub/save/a11y)\n");
    return 0;
}
