#include <library/cpp/getopt/modchooser.h>

int main_solo(int argc, const char** argv);
int diff_main(int argc, const char** argv);

int main(int argc, const char** argv) {
    TModChooser modChooser;

    modChooser.AddMode(
        "solo",
        main_solo,
        "simulate 1 generator vs 1 simulator"
    );

    modChooser.AddMode(
        "grid",
        diff_main,
        "grid comparison generators on different cases"
    );

    return modChooser.Run(argc, argv);
}
