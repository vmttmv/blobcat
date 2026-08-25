#pragma once

#include <stddef.h>

struct bc_obj;

enum bc_format
{
    BC_FORMAT_COFF_X64,
    BC_FORMAT_ELF_X64,
    BC_FORMAT_COUNT     // Internal, not a valid format.
};

struct bc_obj *bc_obj_create(void);
void bc_obj_destroy(struct bc_obj *obj);
void bc_obj_add(struct bc_obj *obj, const char *name, const void *data, size_t size);
void bc_obj_write(struct bc_obj *obj, enum bc_format format, const char *path);
