#include <assert.h>
#define main svg_names_cli_main
#include "../tools/re/svg-names.c"
#undef main

int main(void)
{
    assert(name_crc("") == 0 && name_crc("123456789") == UINT32_C(0xcbf43926));
    assert(name_crc("MenuF.svg") == UINT32_C(0xbae37c8d));
    unsigned char bytes[512] = {0};
    struct image im = { .data = bytes, .size = sizeof bytes };
    im.header.e_phoff = 64; im.header.e_phnum = 2; im.header.e_phentsize = sizeof(Elf32_Phdr);
    im.header.e_shoff = 128; im.header.e_shnum = 3; im.header.e_shentsize = sizeof(Elf32_Shdr);
    Elf32_Phdr first = { .p_type = PT_LOAD, .p_offset = 256, .p_vaddr = 0x1000,
                         .p_filesz = 32, .p_memsz = 64 };
    Elf32_Phdr second = { .p_type = PT_LOAD, .p_offset = 320, .p_vaddr = 0x2000, .p_filesz = 64 };
    memcpy(bytes + 64, &first, sizeof first); memcpy(bytes + 64 + sizeof first, &second, sizeof second);
    size_t available;
    assert(address(&im, 0x1000, &available) == bytes + 256 && available == 32);
    assert(address(&im, 0x101f, &available) == bytes + 287 && available == 1);
    assert(!address(&im, 0x1020, &available)); /* BSS has no file-backed bytes */
    assert(address(&im, 0x2004, &available) == bytes + 324 && available == 60);
    assert(!address(&im, UINT32_MAX, &available) && !address(&im, 0, &available));
    second.p_filesz = 512;
    memcpy(bytes + 64 + sizeof first, &second, sizeof second);
    assert(!address(&im, 0x2004, &available)); /* segment beyond EOF */

    Elf32_Shdr syms = { .sh_type = SHT_DYNSYM, .sh_offset = 384, .sh_size = 2 * sizeof(Elf32_Sym),
                        .sh_entsize = sizeof(Elf32_Sym), .sh_link = 2 };
    Elf32_Shdr strs = { .sh_type = SHT_STRTAB, .sh_offset = 432, .sh_size = 32 };
    memcpy(bytes + 128 + sizeof(Elf32_Shdr), &syms, sizeof syms);
    memcpy(bytes + 128 + 2 * sizeof(Elf32_Shdr), &strs, sizeof strs);
    memcpy(bytes + 432, "\0svg_name_list", 15);
    Elf32_Sym named = { .st_name = 1, .st_value = 0x1004, .st_size = 8, .st_shndx = 1 };
    memcpy(bytes + 384 + sizeof named, &named, sizeof named);
    Elf32_Sym found;
    assert(symbol(&im, "svg_name_list", &found) == 0 && found.st_value == 0x1004 && found.st_size == 8);
    assert(symbol(&im, "absent", &found) == -1);
    named.st_name = 32;
    memcpy(bytes + 384 + sizeof named, &named, sizeof named);
    assert(symbol(&im, "svg_name_list", &found) == -1);
    named.st_name = 1;
    memcpy(bytes + 384 + sizeof named, &named, sizeof named);
    memset(bytes + 432, 'x', 32);
    assert(symbol(&im, "svg_name_list", &found) == -1);
    assert(span(&im, 512, 0) && !span(&im, 512, 1) && !span(&im, SIZE_MAX, 1));
    puts("  ok   resource CRC32 vectors, ELF segment translation/BSS/EOF, bounded dynamic symbols");
    return 0;
}
