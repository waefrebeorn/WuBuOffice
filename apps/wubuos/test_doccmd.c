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

    /* DOC-66: hyperlink with explicit URL */
    if (doccmd_insert_link_url(d, "https://example.com/page") != 1){ fprintf(stderr, "[link_url]\n"); fails++; }
    else {
        /* find the last LINK node and check its target */
        wubumodel_node *last = NULL;
        /* walk all nodes; reuse count_kind-style recursion not available, so
         * check the doc root's direct section children for the newest LINK. */
        wubumodel_node *sec = wubumodel_doc_root(d);
        for (wubumodel_node *s = sec; s; s = wubumodel_node_next_sibling(s))
            for (wubumodel_node *c = wubumodel_node_first_child(s); c; c = wubumodel_node_next_sibling(c))
                if (wubumodel_node_kind(c) == WUBUMODEL_LINK) last = c;
        if (!last || !wubumodel_node_link(last) || strcmp(wubumodel_node_link(last), "https://example.com/page")){
            fprintf(stderr, "[link_url target] '%s'\n", last? wubumodel_node_link(last):"(null)"); fails++;
        }
    }
    /* UXA-47: image with alt text -> stored as a11y note */
    if (doccmd_insert_image_alt(d, "A sample diagram") != 1){ fprintf(stderr, "[image_alt]\n"); fails++; }
    else {
        wubumodel_node *last = NULL;
        wubumodel_node *sec = wubumodel_doc_root(d);
        for (wubumodel_node *s = sec; s; s = wubumodel_node_next_sibling(s))
            for (wubumodel_node *c = wubumodel_node_first_child(s); c; c = wubumodel_node_next_sibling(c))
                if (wubumodel_node_kind(c) == WUBUMODEL_IMAGE) last = c;
        if (!last || !wubumodel_node_note(last) || strcmp(wubumodel_node_note(last), "A sample diagram")){
            fprintf(stderr, "[image_alt note] '%s'\n", last? wubumodel_node_note(last):"(null)"); fails++;
        }
    }
    /* EXP-89: QR code -> image node with payload as note */
    if (doccmd_insert_qr(d, "HELLO-QR") != 1){ fprintf(stderr, "[qr]\n"); fails++; }
    else {
        wubumodel_node *last = NULL;
        wubumodel_node *sec = wubumodel_doc_root(d);
        for (wubumodel_node *s = sec; s; s = wubumodel_node_next_sibling(s))
            for (wubumodel_node *c = wubumodel_node_first_child(s); c; c = wubumodel_node_next_sibling(c))
                if (wubumodel_node_kind(c) == WUBUMODEL_IMAGE) last = c;
        if (!last || !wubumodel_node_note(last) || strcmp(wubumodel_node_note(last), "HELLO-QR")){
            fprintf(stderr, "[qr note] '%s'\n", last? wubumodel_node_note(last):"(null)"); fails++;
        }
    }

    /* INT-3.5: layout-based exporters (PDF/HTML/Markdown/LaTeX/RTF) return a
     * non-null status string and write a file to /tmp. Each builds a
     * wubulayout_doc from the model and calls wubuexp_*. */
    {
        const char *exports[] = {
            "pdf", "pdf_direct", "html", "markdown", "latex", "rtf", NULL
        };
        char *(*fn[])(wubumodel_doc *) = {
            doccmd_export_pdf, doccmd_export_pdf_direct,
            doccmd_export_html, doccmd_export_markdown,
            doccmd_export_latex, doccmd_export_rtf, NULL
        };
        const char *paths[] = {
            "/tmp/wubuos_export.pdf", "/tmp/wubuos_export_direct.pdf",
            "/tmp/wubuos_export.html", "/tmp/wubuos_export.md",
            "/tmp/wubuos_export.tex", "/tmp/wubuos_export.rtf", NULL
        };
        for (int i = 0; exports[i]; i++){
            char *msg = fn[i](d);
            if (!msg){ fprintf(stderr, "[export %s] null status\n", exports[i]); fails++; }
            else {
                FILE *f = fopen(paths[i], "rb");
                if (!f){ fprintf(stderr, "[export %s] file not written: %s\n", exports[i], msg); fails++; }
                else { fclose(f); printf("  export %s: %s\n", exports[i], msg); }
                free(msg);
            }
        }
    }

    /* rtf_runs (src/wuburtf direct write, no layout pass). */
    {
        RtfRun runs[] = {
            { "Hello ", 1, 0, 0 },
            { "bold-italic ", 1, 1, 0 },
            { "mono", 0, 0, 1 },
        };
        char *msg = doccmd_export_rtf_runs(runs, 3);
        if (!msg){ fprintf(stderr, "[export rtf_runs] null status\n"); fails++; }
        else {
            FILE *f = fopen("/tmp/wubuos_export_runs.rtf", "rb");
            if (!f){ fprintf(stderr, "[export rtf_runs] file not written: %s\n", msg); fails++; }
            else {
                fclose(f);
                printf("  export rtf_runs: %s\n", msg);
            }
            free(msg);
        }
    }

    /* redact (src/wuburedact). Walks d's RUNs, marks [0,5). */
    {
        size_t ranges[] = { 0, 5 };
        char *msg = doccmd_redact_doc(d, ranges, 1);
        if (!msg){ fprintf(stderr, "[redact] null status\n"); fails++; }
        else {
            FILE *f = fopen("/tmp/wubuos_redacted.txt", "rb");
            if (!f){ fprintf(stderr, "[redact] file not written: %s\n", msg); fails++; }
            else {
                fclose(f);
                printf("  redact: %s\n", msg);
            }
            free(msg);
        }
    }

    /* col (src/wubucol). */
    {
        char *msg = doccmd_col_demo();
        if (!msg){ fprintf(stderr, "[col] null status\n"); fails++; }
        else {
            printf("  col: %s\n", msg);
            free(msg);
        }
    }

    /* cite (src/wubucite). */
    {
        char *msg = doccmd_cite_demo();
        if (!msg){ fprintf(stderr, "[cite] null status\n"); fails++; }
        else { printf("  cite: %s\n", msg); free(msg); }
    }

    /* caption (src/wubucaption). */
    {
        char *msg = doccmd_caption_demo();
        if (!msg){ fprintf(stderr, "[caption] null status\n"); fails++; }
        else { printf("  caption: %s\n", msg); free(msg); }
    }

    /* heading enforce (src/wubuheading). */
    {
        char *msg = doccmd_heading_enforce(d);
        if (!msg){ fprintf(stderr, "[heading] null status\n"); fails++; }
        else { printf("  heading: %s\n", msg); free(msg); }
    }

    /* eqnum scan (src/wubueqnum). */
    {
        char *msg = doccmd_eqnum_scan(d);
        if (!msg){ fprintf(stderr, "[eqnum] null status\n"); fails++; }
        else { printf("  eqnum: %s\n", msg); free(msg); }
    }

    /* vars expand (src/wubuvars). */
    {
        char *msg = doccmd_vars_expand(NULL);
        if (!msg){ fprintf(stderr, "[vars] null status\n"); fails++; }
        else { printf("  vars: %s\n", msg); free(msg); }
    }

    /* sha256 (src/wubuhash). */
    {
        char *msg = doccmd_hash_sha256("WuBuOffice");
        if (!msg){ fprintf(stderr, "[hash] null status\n"); fails++; }
        else { printf("  hash: %s\n", msg); free(msg); }
    }

    /* doc sig (src/wubusig + wubuhash). */
    {
        char *msg = doccmd_doc_sign();
        if (!msg){ fprintf(stderr, "[sig] null status\n"); fails++; }
        else { printf("  sig: %s\n", msg); free(msg); }
    }

    /* crdt (src/wubucrdt). */
    {
        char *msg = doccmd_crdt_demo();
        if (!msg){ fprintf(stderr, "[crdt] null status\n"); fails++; }
        else { printf("  crdt: %s\n", msg); free(msg); }
    }

    /* csv (src/wubucsv). */
    {
        char *msg = doccmd_csv_parse();
        if (!msg){ fprintf(stderr, "[csv] null status\n"); fails++; }
        else { printf("  csv: %s\n", msg); free(msg); }
    }

    /* focus (src/wubufocus). */
    {
        char *msg = doccmd_focus_demo();
        if (!msg){ fprintf(stderr, "[focus] null status\n"); fails++; }
        else { printf("  focus: %s\n", msg); free(msg); }
    }

    /* watermark (src/wubuwatermark). */
    {
        char *msg = doccmd_watermark_demo();
        if (!msg){ fprintf(stderr, "[watermark] null status\n"); fails++; }
        else { printf("  watermark: %s\n", msg); free(msg); }
    }

    /* dyslexia (src/wubudyslexia). */
    {
        char *msg = doccmd_dyslexia_demo();
        if (!msg){ fprintf(stderr, "[dyslexia] null status\n"); fails++; }
        else { printf("  dyslexia: %s\n", msg); free(msg); }
    }

    /* fmtpaint (src/wubufmtpaint). */
    {
        char *msg = doccmd_fmtpaint_demo(d);
        if (!msg){ fprintf(stderr, "[fmtpaint] null status\n"); fails++; }
        else { printf("  fmtpaint: %s\n", msg); free(msg); }
    }

    /* sandbox (src/wubusandbox). */
    {
        char *msg = doccmd_sandbox_demo();
        if (!msg){ fprintf(stderr, "[sandbox] null status\n"); fails++; }
        else { printf("  sandbox: %s\n", msg); free(msg); }
    }

    /* form (src/wubuform). */
    {
        char *msg = doccmd_form_demo();
        if (!msg){ fprintf(stderr, "[form] null status\n"); fails++; }
        else { printf("  form: %s\n", msg); free(msg); }
    }

    /* history (src/wubuhistory). */
    {
        char *msg = doccmd_history_demo();
        if (!msg){ fprintf(stderr, "[history] null status\n"); fails++; }
        else { printf("  history: %s\n", msg); free(msg); }
    }

    /* lang (src/wubulang). */
    {
        char *msg = doccmd_lang_demo();
        if (!msg){ fprintf(stderr, "[lang] null status\n"); fails++; }
        else { printf("  lang: %s\n", msg); free(msg); }
    }

    /* nesttab (src/wubunesttab). */
    {
        char *msg = doccmd_nesttab_demo(d);
        if (!msg){ fprintf(stderr, "[nesttab] null status\n"); fails++; }
        else { printf("  nesttab: %s\n", msg); free(msg); }
    }

    /* pdfextract (src/wubupdfextract). */
    {
        char *msg = doccmd_pdfextract_demo();
        if (!msg){ fprintf(stderr, "[pdfextract] null status\n"); fails++; }
        else { printf("  pdfextract: %s\n", msg); free(msg); }
    }

    /* pdfform (src/wubupdfform + wubuform). */
    {
        char *msg = doccmd_pdfform_demo();
        if (!msg){ fprintf(stderr, "[pdfform] null status\n"); fails++; }
        else { printf("  pdfform: %s\n", msg); free(msg); }
    }

    /* scope (src/wubuscope). */
    {
        char *msg = doccmd_scope_demo();
        if (!msg){ fprintf(stderr, "[scope] null status\n"); fails++; }
        else { printf("  scope: %s\n", msg); free(msg); }
    }

    /* sync (src/wubusync). */
    {
        char *msg = doccmd_sync_demo();
        if (!msg){ fprintf(stderr, "[sync] null status\n"); fails++; }
        else { printf("  sync: %s\n", msg); free(msg); }
    }

    /* xps (src/wubuxps). */
    {
        char *msg = doccmd_xps_demo();
        if (!msg){ fprintf(stderr, "[xps] null status\n"); fails++; }
        else { printf("  xps: %s\n", msg); free(msg); }
    }

    /* aislot (src/wubuaislot). */
    {
        char *msg = doccmd_aislot_demo();
        if (!msg){ fprintf(stderr, "[aislot] null status\n"); fails++; }
        else { printf("  aislot: %s\n", msg); free(msg); }
    }

    /* spreadsheet & document analysis wave (2026-08-11): sort/filter/subtotal/
     * goalseek/solver/pivot/scenario/freeze/hyperlink/thesaurus/grammar/
     * index/mailmerge/diff/masterdoc. */
    {
        const char *names[] = {
            "sort","filter","subtotal","goalseek","solver","pivot","scenario",
            "freeze","hyperlink","thesaurus","grammar","index","mailmerge",
            "diff","masterdoc","dropcap","ruler","gridline","icon","gallery",
            "sidebar","transition","animation","masterslide","connector",
            "encrypt","mailexport","notebookbar","qr","smartart",
            "basic","3d"
        };
        char *(*fns[])(void) = {
            doccmd_sort_demo, doccmd_filter_demo, doccmd_subtotal_demo,
            doccmd_goalseek_demo, doccmd_solver_demo, doccmd_pivot_demo,
            doccmd_scenario_demo, doccmd_freeze_demo, doccmd_hyperlink_demo,
            doccmd_thesaurus_demo, doccmd_grammar_demo, doccmd_index_demo,
            doccmd_mailmerge_demo, doccmd_diff_demo, doccmd_masterdoc_demo,
            doccmd_dropcap_demo, doccmd_ruler_demo, doccmd_gridline_demo,
            doccmd_icon_demo, doccmd_gallery_demo, doccmd_sidebar_demo,
            doccmd_transition_demo, doccmd_animation_demo, doccmd_masterslide_demo,
            doccmd_connector_demo, doccmd_encrypt_demo, doccmd_mailexport_demo,
            doccmd_notebookbar_demo, doccmd_qr_demo, doccmd_smartart_demo,
            doccmd_basic_demo, doccmd_3d_demo
        };
        for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
            char *msg = fns[i]();
            if (!msg){ fprintf(stderr, "[%s] null status\n", names[i]); fails++; }
            else { printf("  %s: %s\n", names[i], msg); free(msg); }
        }
    }

    /* ---- DEPTH CHECK: round-trip load -> save -> reload for every format
     * the doc view can open. A missing writer (or a save that silently becomes
     * a different format) breaks editing usability. Use a FRESH simple doc so
     * the reloaded text is unambiguous (the main test doc is heavily mutated). */
    {
        const char *formats[] = { "docx", "odt", "rtf", "epub" };
        for (size_t i = 0; i < sizeof formats / sizeof formats[0]; i++){
            wubumodel_doc *src = wubumodel_doc_create();
            wubumodel_node *sec  = wubumodel_node_create(src, WUBUMODEL_SECTION);
            wubumodel_node *para = wubumodel_node_create(src, WUBUMODEL_PARAGRAPH);
            wubumodel_node *run  = wubumodel_node_create(src, WUBUMODEL_RUN);
            wubumodel_run_set_text(run, "RoundTripCheck");
            wubumodel_node_append(src, para, run);
            wubumodel_node_append(src, sec, para);
            char path[128];
            snprintf(path, sizeof path, "/tmp/wubuos_roundtrip.%s", formats[i]);
            char *sv = doccmd_save(src, path);
            if (!sv || !strstr(sv, "saved")){
                fprintf(stderr, "[roundtrip %s] save failed: %s\n", formats[i], sv?sv:"null");
                if(sv) free(sv); wubumodel_doc_destroy(src); fails++; continue;
            }
            printf("  roundtrip %s save: OK\n", formats[i]); free(sv);
            wubumodel_doc_destroy(src);
            /* reload and confirm text survived (recursive walk) */
            wubumodel_doc *rt = NULL;
            int lrc = -1;
            if (!strcmp(formats[i],"docx")) lrc = wubumodel_load_docx(path, &rt);
            else if (!strcmp(formats[i],"odt")) lrc = wubumodel_load_odt(path, &rt);
            else if (!strcmp(formats[i],"rtf")) lrc = wubumodel_load_rtf(path, &rt);
            else if (!strcmp(formats[i],"epub")) lrc = wubumodel_load_epub(path, &rt);
            if (lrc != 0 || !rt){ fprintf(stderr, "[roundtrip %s] reload failed (rc=%d)\n", formats[i], lrc); fails++; continue; }
            int found = 0;
            /* recursive DFS over the whole tree */
            wubumodel_node *st[512]; int sp=0;
            for (wubumodel_node *s = wubumodel_doc_root(rt); s; s = wubumodel_node_next_sibling(s)){
                if (sp < 512) st[sp++] = s;
            }
            while (sp){
                wubumodel_node *n = st[--sp];
                if (wubumodel_node_kind(n)==WUBUMODEL_RUN &&
                    wubumodel_run_text(n) && strstr(wubumodel_run_text(n),"RoundTripCheck"))
                    found = 1;
                for (wubumodel_node *c = wubumodel_node_first_child(n); c; c = wubumodel_node_next_sibling(c))
                    if (sp < 512) st[sp++] = c;
            }
            if (!found){ fprintf(stderr, "[roundtrip %s] text lost\n", formats[i]); fails++; }
            else printf("  roundtrip %s: text survived reload OK\n", formats[i]);
            wubumodel_doc_destroy(rt);
        }
    }

    wubumodel_doc_destroy(d);

    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: doccmd (15 structural inserts + script field + epub/save/a11y + 6 layout exporters + rtf_runs/redact/col + cite/caption/heading/eqnum/vars/hash/sig/crdt + csv/focus/watermark/dyslexia/fmtpaint/sandbox + form/history/lang/nesttab/pdfextract/pdfform/scope/sync/xps/aislot)\n");
    return 0;
}
