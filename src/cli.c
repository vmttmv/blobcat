#include "blobcat.h"

#define ADDOBJ(obj, sym, str)        bc_obj_add((obj), (sym), (str), sizeof(str))

int main(int argc, char *argv[])
{
    struct bc_obj *obj = bc_obj_create();
    ADDOBJ(obj, "_sym1", "This is sym1");
    ADDOBJ(obj, "_sym2", "This is sym2\nHello.");
    ADDOBJ(obj, "_sym3", "This is sym3\n\nNOHello.");
    bc_obj_write(obj, BC_FORMAT_COFF_X64, "foo.obj");

    return 0;
}
