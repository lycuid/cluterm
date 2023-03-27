#include <cluterm/terminal.h>

int main(void)
{
    Terminal term;
    term_init(&term);
    term_start(&term);
    term_destroy(&term);
    return 0;
}
