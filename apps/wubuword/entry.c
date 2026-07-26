#include <string.h>
extern int wubuword_main(int argc, char **argv);
extern int wubuword_spell_main(int argc, char **argv);

int main(int argc, char **argv) {
    /* subcommand: spell-check a text file (wubuword spell <file> [dict]) */
    if (argc > 1 && strcmp(argv[1], "spell") == 0)
        return wubuword_spell_main(argc, argv);
    return wubuword_main(argc, argv);
}
