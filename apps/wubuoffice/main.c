#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int wubuword_main(int argc, char **argv);
extern int wubucell_main(int argc, char **argv);
extern int wubushow_main(int argc, char **argv);
extern int wuburead_main(int argc, char **argv);

static void usage(const char *prog) {
    fprintf(stderr,
        "WuBuOffice — ground-up C11 SLERM of OOXML\n"
        "usage: %s <command> [args]\n"
        "  word  [out.docx]            generate a rich .docx\n"
        "  cell  [out.xlsx]            generate a .xlsx workbook\n"
        "  show  [out.pptx]            generate a .pptx deck\n"
        "  read  <file.docx|xlsx|pptx> read + extract text from any OOXML\n",
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
    usage(argv[0]);
    return 1;
}
