#include <stdio.h>

extern char _sym1[];
extern char _sym2[];
extern char _sym3[];

int main(void) {
    printf("sym1: %s\n", _sym1);
    printf("sym2: %s\n", _sym2);
    printf("sym3: %s\n", _sym3);
    return 0;
}
