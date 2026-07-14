#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int wubuword_main(int argc, char **argv);
extern int wubucell_main(int argc, char **argv);
extern int wubushow_main(int argc, char **argv);
extern int wuburead_main(int argc, char **argv);
extern int wubuedit_main(int argc, char **argv);
extern int wubuconv_convert(const char *in_path, const char *out_path);

static void usage(const char *prog) {
    fprintf(stderr,
        "WuBuOffice — ground-up C11 SLERM of OOXML + ODF\n"
        "usage: %s <command> [args]\n"
        "  word  [out.docx]                       generate a rich .docx\n"
        "  cell  [out.xlsx]                       generate a .xlsx workbook\n"
        "  show  [out.pptx]                       generate a .pptx deck\n"
        "  read  <file.docx|xlsx|pptx>            read + extract text from any OOXML\n"
        "  edit  <in.docx|xlsx|pptx> [out.<ext>]  round-trip re-write (structure preserved)\n"
        "  convert <in> <out>                     convert ANY supported format to ANY other\n"
        "         supported in:  docx xlsx pptx csv tsv md odt ods odp fodt fods fodp doc xls ppt\n"
        "         supported out: docx xlsx pptx csv tsv md html rtf odt ods odp fodt fods fodp doc xls ppt pdf epub json\n",
        prog);
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 1; }
    const char *cmd = argv[1];
    /* shift argv so subcommands see their own args */
    argv[1] = argv[0];
    argc--;
    argv++;
    if (strcmp(cmd, "word") == 0) return wubuword_main(argc, argv);
    if (strcmp(cmd, "cell") == 0) return wubucell_main(argc, argv);
    if (strcmp(cmd, "show") == 0) return wubushow_main(argc, argv);
    if (strcmp(cmd, "read") == 0) return wuburead_main(argc, argv);
    if (strcmp(cmd, "edit") == 0) return wubuedit_main(argc, argv);
    if (strcmp(cmd, "convert") == 0) {
        if (argc < 3) { fprintf(stderr, "usage: %s convert <in> <out>\n", argv[0]); return 1; }
        return wubuconv_convert(argv[1], argv[2]) ? 1 : 0;
    }
    usage(argv[0]);
    return 1;
}
