#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <Windows.h>
#include <winnt.h>

static void write_coff(const char *filename, const char *sym, const void *data, size_t size);

int main(int argc, char *argv[])
{
    const char data[] = "Hello world, it's a me mario.\nHope this is good.";
    const size_t size = strlen(data);

    write_coff("out.obj", "mario", data, size+1);

    return 0;
}

void write_coff(const char *filename, const char *sym, const void *data, size_t size)
{
    FILE *fp;

    fopen_s(&fp, filename, "wb");

    size_t name_len = strlen(sym);
    DWORD strtab_size = sizeof(DWORD) + (DWORD)(name_len + 1);

    IMAGE_FILE_HEADER header = {0};
    IMAGE_SECTION_HEADER section = {0};
    IMAGE_SYMBOL symbol = {0};

    DWORD section_data_offset = sizeof(header) + sizeof(section);
    DWORD symtab_offset = section_data_offset + (DWORD)size;
    DWORD strtab_offset = symtab_offset + sizeof(symbol);

    header.Machine = IMAGE_FILE_MACHINE_AMD64;
    header.NumberOfSections = 1;
    header.NumberOfSymbols = 1;
    header.PointerToSymbolTable = symtab_offset;

    memcpy(section.Name, ".rdata", 6);
    section.PointerToRawData = section_data_offset;
    section.SizeOfRawData = (DWORD)size;
    section.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;

    symbol.N.Name.Short = 0;
    symbol.N.Name.Long = sizeof(DWORD);
    symbol.SectionNumber = 1;
    symbol.StorageClass = IMAGE_SYM_CLASS_EXTERNAL;

    fwrite(&header, sizeof(header), 1, fp);
    fwrite(&section, sizeof(section), 1, fp);
    fwrite(data, size, 1, fp);
    fwrite(&symbol, sizeof(symbol), 1, fp);
    fwrite(&strtab_size, sizeof(strtab_size), 1, fp);
    fwrite(sym, name_len + 1, 1, fp);
    fclose(fp);
}
