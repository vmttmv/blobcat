#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "blobcat.h"

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct bc_sym
{
    const char  *name;
    size_t      name_len;
    const void  *data;
    size_t      size;
};

struct bc_obj
{
    size_t          sym_count;
    size_t          sym_cap;
    struct bc_sym   *syms;
};

#pragma pack(push, 1)

//
// COFF64
//
#define COFF64_IMAGE_FILE_MACHINE_AMD64         0x8664
#define COFF64_IMAGE_SCN_CNT_INITIALIZED_DATA   0x00000040
#define COFF64_IMAGE_SCN_MEM_READ               0x40000000
#define COFF64_IMAGE_SYM_CLASS_EXTERNAL         2 

struct coff64_file_header
{
    uint16_t    Machine;
    uint16_t    NumberOfSections;
    uint32_t    TimeDateStamp;
    uint32_t    PointerToSymbolTable;
    uint32_t    NumberOfSymbols;
    uint16_t    SizeOfOptionalHeader;
    uint16_t    Characteristics;
};

struct coff64_section_header
{
    uint8_t         Name[8];

    union
    {
        uint32_t    PhysicalAddress;
        uint32_t    VirtualSize;
    } Misc;

    uint32_t        VirtualAddress;
    uint32_t        SizeOfRawData;
    uint32_t        PointerToRawData;
    uint32_t        PointerToRelocations;
    uint32_t        PointerToLinenumbers;
    uint16_t        NumberOfRelocations;
    uint16_t        NumberOfLinenumbers;
    uint32_t        Characteristics;
};

struct coff64_symbol
{
    union
    {
        uint8_t         Short[8];
        struct
        {
            uint32_t    Zeroes;
            uint32_t    Offset;
        };
    } Name;

    uint32_t        Value;
    uint16_t        SectionNumber;
    uint16_t        Type;
    uint8_t         StorageClass;
    uint8_t         NumberOfAuxSymbols;
};

//
// ELF64
//
#define ELF64_EI_NIDENT             16

#define ELF64_STV_DEFAULT           0   /* Default symbol visibility rules */

#define ELF64_ST_INFO(bind, type) \
    (((bind) << 4) + ((type) & 0xf))

#define ELF64_STB_GLOBAL	        1	/* Global symbol */
#define ELF64_STT_OBJECT            1	/* Symbol is a data object */

#define ELF64_ELFMAG                "\177ELF"
#define ELF64_SELFMAG               4

#define EI_CLASS                    4		/* File class byte index */
#define ELFCLASS64                  2		/* 64-bit objects */

#define EI_DATA	                    5		/* Data encoding byte index */
#define ELFDATA2LSB                 1		/* 2's complement, little endian */

#define EI_VERSION                  6		/* File version byte index */
				                            /* Value must be EV_CURRENT */
#define EV_CURRENT                  1		/* Current version */

#define EI_OSABI                    7		/* OS ABI identification */
#define ELFOSABI_SYSV		        0

#define ET_REL                      1		/* Relocatable file */
#define EM_X86_64                   62      /* AMD x86-64 architecture */

#define SHT_NULL                    0		/* Section header table entry unused */
#define SHT_PROGBITS                1		/* Program data */
#define SHT_SYMTAB                  2		/* Symbol table */
#define SHT_STRTAB                  3		/* String table */

// shdr flags
#define SHF_WRITE	                (1 << 0)	/* Writable */
#define SHF_ALLOC	                (1 << 1)	/* Occupies memory during execution */
#define SHF_EXECINSTR	            (1 << 2)	/* Executable */
#define SHF_MERGE	                (1 << 4)	/* Might be merged */
#define SHF_STRINGS	                (1 << 5)	/* Contains nul-terminated strings */

/*
 * Section indices:
 *
 *   0 = NULL
 *   1 = .rodata
 *   2 = .symtab
 *   3 = .strtab
 *   4 = .shstrtab
 */
enum
{
    SH_NULL,
    SH_RODATA,
    SH_SYMTAB,
    SH_STRTAB,
    SH_SHSTRTAB,
    SH_COUNT
};

struct elf64_ehdr
{
   uint8_t  e_ident[ELF64_EI_NIDENT];
   uint16_t e_type;
   uint16_t e_machine;
   uint32_t e_version;
   uint64_t e_entry;
   uint64_t e_phoff;
   uint64_t e_shoff;
   uint32_t e_flags;
   uint16_t e_ehsize;
   uint16_t e_phentsize;
   uint16_t e_phnum;
   uint16_t e_shentsize;
   uint16_t e_shnum;
   uint16_t e_shstrndx;
};

struct elf64_shdr
{
  uint64_t  sh_name;		/* Section name (string tbl index) */
  uint64_t	sh_type;		/* Section type */
  uint64_t	sh_flags;		/* Section flags */
  uint64_t	sh_addr;		/* Section virtual addr at execution */
  uint64_t	sh_offset;		/* Section file offset */
  uint64_t	sh_size;		/* Section size in bytes */
  uint64_t	sh_link;		/* Link to another section */
  uint64_t	sh_info;		/* Additional section information */
  uint64_t	sh_addralign;	/* Section alignment */
  uint64_t	sh_entsize;		/* Entry size if section holds table */
};

struct elf64_sym
{
    uint32_t    st_name;
    uint8_t     st_info;
    uint8_t     st_other;
    uint16_t    st_shndx;
    uint64_t    st_value;
    uint64_t    st_size;
};
#pragma pack(pop)

static void *bc_xcalloc(size_t size);
static void *bc_xrealloc(void *ptr, size_t size);

static void bc_write_coff_x64(const struct bc_obj *obj, const char *path);
static void bc_write_elf_x64(const struct bc_obj *obj, const char *path);

struct bc_obj *bc_obj_create(void)
{
    struct bc_obj *obj = bc_xcalloc(sizeof(*obj));

    return obj;
}

void bc_obj_destroy(struct bc_obj *obj)
{
    assert(obj);
    free(obj->syms);
    free(obj);
}

void bc_obj_add(struct bc_obj *obj, const char *name, const void *data, size_t size)
{
    assert(obj);
    assert(name);
    assert(data);
    assert(size > 0);

    if (obj->sym_count == obj->sym_cap) {
        obj->sym_cap = obj->sym_cap ? obj->sym_cap * 2 : 1;
        obj->syms = bc_xrealloc(obj->syms, obj->sym_cap * sizeof(struct bc_sym));
    }

    struct bc_sym *sym = obj->syms + obj->sym_count++;
    sym->name = name;
    sym->name_len = strlen(name) + 1;
    sym->data = data;
    sym->size = size;
}

void *bc_xcalloc(size_t size)
{
    void *ptr = calloc(1, size);
    if (!ptr) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    return ptr;
}

void *bc_xrealloc(void *ptr, size_t size)
{
    ptr = realloc(ptr, size);
    if (!ptr) {
        perror("realloc");
        exit(EXIT_FAILURE);
    }

    return ptr;
}

void bc_obj_write(struct bc_obj *obj, enum bc_format format, const char *path)
{
    assert(obj);
    assert(format >= 0 && format < BC_FORMAT_COUNT);

    switch (format) {
        case BC_FORMAT_COFF_X64: {
            bc_write_coff_x64(obj, path);
            break;
        }

        case BC_FORMAT_ELF_X64: {
            bc_write_elf_x64(obj, path);
            break;
        }

        default: {
            fprintf(stderr, "bc: Invalid format: %d\n", format);
        }
    }
}

void bc_write_coff_x64(const struct bc_obj *obj, const char *path)
{
    uint32_t data_size = 0;
    uint32_t strtab_size = 0;
    for (size_t i = 0; i < obj->sym_count; ++i) {
        data_size += obj->syms[i].size;
        strtab_size += sizeof(uint32_t) + obj->syms[i].name_len;
    }

    assert(data_size);
    assert(strtab_size);

    size_t symtab_size = sizeof(struct coff64_symbol) * obj->sym_count;
    assert(symtab_size);
    struct coff64_symbol *symtab = bc_xcalloc(symtab_size);

    char *strtab = bc_xcalloc(strtab_size);
    *(uint32_t *)strtab = strtab_size;

    uint32_t str_offset = sizeof(uint32_t);
    uint32_t val_offset = 0;
    for (size_t i = 0; i < obj->sym_count; ++i) {
        memcpy(strtab + str_offset, obj->syms[i].name, obj->syms[i].name_len);

        symtab[i].Value = val_offset;
        symtab[i].Name.Offset = str_offset;
        symtab[i].SectionNumber = 1;
        symtab[i].StorageClass = COFF64_IMAGE_SYM_CLASS_EXTERNAL;

        str_offset += obj->syms[i].name_len;
        val_offset += obj->syms[i].size;
    }

    uint32_t data_offset = sizeof(struct coff64_file_header) + sizeof(struct coff64_section_header); 
    uint32_t symtab_offset = data_offset + data_size;
    uint32_t strtab_offset = symtab_offset + sizeof(struct coff64_symbol) * obj->sym_count;

    struct coff64_file_header header = {0};
    header.Machine = COFF64_IMAGE_FILE_MACHINE_AMD64;
    header.NumberOfSections = 1;
    header.NumberOfSymbols = (uint32_t)obj->sym_count;
    header.PointerToSymbolTable = symtab_offset;

    struct coff64_section_header section = {".rdata"};
    section.PointerToRawData = data_offset;
    section.SizeOfRawData = data_size;
    section.Characteristics = COFF64_IMAGE_SCN_CNT_INITIALIZED_DATA | COFF64_IMAGE_SCN_MEM_READ;

    FILE *fp = fopen(path, "wb");
    assert(fp);

    fwrite(&header, sizeof(header), 1, fp);
    fwrite(&section, sizeof(section), 1, fp);

    for (size_t i = 0; i < obj->sym_count; ++i) {
        const struct bc_sym *sym = obj->syms + i;
        fwrite(sym->data, sym->size, 1, fp);
    }

    fwrite(symtab, symtab_size, 1, fp);
    fwrite(strtab, strtab_size, 1, fp);
    fclose(fp);

    free(symtab);
    free(strtab);
}

void bc_write_elf_x64(const struct bc_obj *obj, const char *path)
{
    size_t data_size = 0;
    size_t strtab_size = 1;
    for (size_t i = 0; i < obj->sym_count; ++i) {
        data_size += obj->syms[i].size;
        strtab_size += obj->syms[i].name_len;
    }

    size_t syms_size = sizeof(struct elf64_sym) * (obj->sym_count + 1);
    struct elf64_sym *syms = bc_xcalloc(syms_size);

    // Symbol name strtab
    char *strtab = bc_xcalloc(strtab_size);
    size_t str_offset = 1;
    size_t val_offset = 0;
    for (size_t i = 0; i < obj->sym_count; ++i) {
        memcpy(strtab + str_offset, obj->syms[i].name, obj->syms[i].name_len);

        struct elf64_sym *sym = syms + i + 1;
        sym->st_name = str_offset;
        sym->st_info = ELF64_ST_INFO(ELF64_STB_GLOBAL, ELF64_STT_OBJECT);
        sym->st_other = ELF64_STV_DEFAULT;
        sym->st_shndx = SH_RODATA;
        sym->st_value = val_offset;

        str_offset += obj->syms[i].name_len;
        val_offset += obj->syms[i].size;
    }

    // Section name strtab
    const char shstrtab[] =
        "\0"
        ".rodata\0"
        ".symtab\0"
        ".strtab\0"
        ".shstrtab\0";

    struct elf64_ehdr header = {0};
    memcpy(header.e_ident, ELF64_ELFMAG, ELF64_SELFMAG);
    header.e_ident[EI_CLASS] = ELFCLASS64;
    header.e_ident[EI_DATA] = ELFDATA2LSB;
    header.e_ident[EI_VERSION] = EV_CURRENT;
    header.e_ident[EI_OSABI] = ELFOSABI_SYSV;
    header.e_type = ET_REL;
    header.e_machine = EM_X86_64;
    header.e_version = EV_CURRENT;
    header.e_ehsize = sizeof(header);
    header.e_shentsize = sizeof(struct elf64_shdr);
    header.e_shnum = SH_COUNT;
    header.e_shstrndx = SH_SHSTRTAB;

    size_t offset = sizeof(header);

    size_t rodata_offset = offset;
    offset += data_size;

    // align symbol table to 8 bytes.
    offset = (offset + 7) & ~7ULL;

    size_t symtab_offset = offset;
    offset += syms_size;

    size_t strtab_offset = offset;
    offset += strtab_size;

    size_t shstrtab_offset = offset;
    offset += sizeof(shstrtab);

    size_t shdr_offset = offset;

    struct elf64_shdr shdr[SH_COUNT] = {0};

    /*
     * .rodata
     */
    shdr[SH_RODATA].sh_name = 1;
    shdr[SH_RODATA].sh_type = SHT_PROGBITS;
    shdr[SH_RODATA].sh_flags = SHF_ALLOC;
    shdr[SH_RODATA].sh_addr = 0;
    shdr[SH_RODATA].sh_offset = rodata_offset;
    shdr[SH_RODATA].sh_size = data_size;
    shdr[SH_RODATA].sh_addralign = 1;

    /*
     * .symtab
     */
    shdr[SH_SYMTAB].sh_name = 9;
    shdr[SH_SYMTAB].sh_type = SHT_SYMTAB;
    shdr[SH_SYMTAB].sh_offset = symtab_offset;
    shdr[SH_SYMTAB].sh_size = syms_size;
    shdr[SH_SYMTAB].sh_link = SH_STRTAB;
    shdr[SH_SYMTAB].sh_info = 1;
    shdr[SH_SYMTAB].sh_addralign = 8;
    shdr[SH_SYMTAB].sh_entsize = sizeof(struct elf64_sym);

    /*
     * .strtab
     */
    shdr[SH_STRTAB].sh_name = 17;
    shdr[SH_STRTAB].sh_type = SHT_STRTAB;
    shdr[SH_STRTAB].sh_offset = strtab_offset;
    shdr[SH_STRTAB].sh_size = strtab_size;
    shdr[SH_STRTAB].sh_addralign = 1;

    /*
     * .shstrtab
     */
    shdr[SH_SHSTRTAB].sh_name = 25;
    shdr[SH_SHSTRTAB].sh_type = SHT_STRTAB;
    shdr[SH_SHSTRTAB].sh_offset = shstrtab_offset;
    shdr[SH_SHSTRTAB].sh_size = sizeof(shstrtab);
    shdr[SH_SHSTRTAB].sh_addralign = 1;

    /*
     * Write ELF header.
     */
    FILE *fp = fopen(path, "wb");
    fwrite(&header, sizeof(header), 1, fp);

    /*
     * Write .rodata.
     */
    for (size_t i = 0; i < obj->sym_count; ++i)
        fwrite(obj->syms[i].data, obj->syms[i].size, 1, fp);

    /*
     * Padding before .symtab.
     */
    size_t current = sizeof(struct elf64_ehdr) + data_size;
    while (current < symtab_offset) {
        fputc(0, fp);
        current++;
    }

    // Symbol table.
    fwrite(syms, syms_size, 1, fp);

    // String table.
    fwrite(strtab, strtab_size, 1, fp);

    // Section-name string table.
    fwrite(shstrtab, sizeof(shstrtab), 1, fp);

    // Section headers.
    fwrite(shdr, sizeof(shdr), 1, fp);

    /*
     * Fix up the ELF header's section-header offset.
     *
     * We need to rewrite the header because we only know this
     * offset after laying everything out.
     */
    header.e_shoff = shdr_offset;

    fseek(fp, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, fp);

    free(strtab);
    fclose(fp);
}
