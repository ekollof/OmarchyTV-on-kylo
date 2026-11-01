/* Resolve this firmware's BinSVG resource IDs/names via svg_name_list in the
 * ELF dynamic symbols. Verify every name against the pack index CRC32, rather
 * than guessing style names from geometry or grepping string fragments. */
#include <elf.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct image { unsigned char *data; size_t size; Elf32_Ehdr header; };
static int span(const struct image *im, size_t off, size_t size)
{ return off <= im->size && size <= im->size - off; }
static int section(const struct image *im, unsigned n, Elf32_Shdr *out)
{
    size_t off = im->header.e_shoff + (size_t)n * im->header.e_shentsize;
    if (n >= im->header.e_shnum || !span(im, off, sizeof *out)) return -1;
    memcpy(out, im->data + off, sizeof *out);
    return 0;
}
static int symbol(const struct image *im, const char *name, Elf32_Sym *out)
{
    for (unsigned i = 0; i < im->header.e_shnum; i++) {
        Elf32_Shdr syms, strs;
        if (section(im, i, &syms) != 0) return -1;
        if (syms.sh_type != SHT_DYNSYM) continue;
        if (syms.sh_entsize != sizeof(Elf32_Sym) ||
            !span(im, syms.sh_offset, syms.sh_size) ||
            section(im, syms.sh_link, &strs) != 0 ||
            strs.sh_type != SHT_STRTAB || !span(im, strs.sh_offset, strs.sh_size)) return -1;
        for (size_t at = 0; at + sizeof *out <= syms.sh_size; at += sizeof *out) {
            Elf32_Sym s;
            memcpy(&s, im->data + syms.sh_offset + at, sizeof s);
            if (s.st_name >= strs.sh_size) return -1;
            const char *text = (const char *)im->data + strs.sh_offset + s.st_name;
            if (!memchr(text, 0, strs.sh_size - s.st_name)) return -1;
            if (!strcmp(text, name) && s.st_shndx != SHN_UNDEF) { *out = s; return 0; }
        }
    }
    return -1;
}
static const unsigned char *address(const struct image *im, uint32_t addr, size_t *available)
{
    for (unsigned i = 0; i < im->header.e_phnum; i++) {
        Elf32_Phdr p;
        size_t off = im->header.e_phoff + (size_t)i * im->header.e_phentsize;
        if (!span(im, off, sizeof p)) return NULL;
        memcpy(&p, im->data + off, sizeof p);
        if (p.p_type != PT_LOAD || addr < p.p_vaddr) continue;
        uint32_t delta = addr - p.p_vaddr;
        if (delta >= p.p_filesz || !span(im, p.p_offset, p.p_filesz)) continue;
        *available = p.p_filesz - delta;
        return im->data + p.p_offset + delta;
    }
    return NULL;
}
static uint32_t le32(const unsigned char *p)
{ return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t name_crc(const char *text)
{
    uint32_t crc = UINT32_MAX;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        crc ^= *p;
        for (unsigned i = 0; i < 8; i++) crc = (crc >> 1) ^ ((crc & 1) ? UINT32_C(0xedb88320) : 0);
    }
    return ~crc;
}
int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s aurora.elf telefunken18_svg.txt\n", argv[0]);
        return 2;
    }
    struct image im = {0};
    FILE *elf = fopen(argv[1], "rb"), *index = fopen(argv[2], "r");
    int rc = 1;
    if (!elf || !index || fseek(elf, 0, SEEK_END) != 0) goto done;
    long length = ftell(elf);
    if (length < (long)sizeof im.header || length > 64 * 1024 * 1024 || fseek(elf, 0, SEEK_SET) != 0)
        goto done;
    im.size = (size_t)length;
    im.data = malloc(im.size);
    if (!im.data || fread(im.data, 1, im.size, elf) != im.size) goto done;
    memcpy(&im.header, im.data, sizeof im.header);
    if (memcmp(im.header.e_ident, ELFMAG, SELFMAG) || im.header.e_ident[EI_CLASS] != ELFCLASS32 ||
        im.header.e_ident[EI_DATA] != ELFDATA2LSB || im.header.e_type != ET_EXEC ||
        im.header.e_machine != EM_ARM || im.header.e_phentsize != sizeof(Elf32_Phdr) ||
        im.header.e_shentsize != sizeof(Elf32_Shdr)) goto done;
    if (!span(&im, im.header.e_phoff, (size_t)im.header.e_phnum * sizeof(Elf32_Phdr)) ||
        !span(&im, im.header.e_shoff, (size_t)im.header.e_shnum * sizeof(Elf32_Shdr))) goto done;
    Elf32_Sym table;
    if (symbol(&im, "svg_name_list", &table) != 0 ||
        ELF32_ST_TYPE(table.st_info) != STT_OBJECT || !table.st_size || table.st_size % 4) goto done;
    size_t available;
    const unsigned char *p = address(&im, table.st_value, &available);
    if (!p || table.st_size > available) goto done;
    size_t count;
    char line[256], extra;
    if (!fgets(line, sizeof line, index) || sscanf(line, "%zu %c", &count, &extra) != 1 ||
        count != table.st_size / 4) goto done;
    puts("resource_id\trecord_hash\tname");
    for (size_t i = 0; i < count; i++) {
        uint32_t expected;
        size_t record_length;
        if (!fgets(line, sizeof line, index) ||
            sscanf(line, "%" SCNx32 " %zu %c", &expected, &record_length, &extra) != 2 || !record_length)
            goto done;
        const unsigned char *name = address(&im, le32(p + i * 4), &available);
        if (!name || !memchr(name, 0, available) || name_crc((const char *)name) != expected) {
            fprintf(stderr, "name/index mismatch at resource %zu\n", i);
            goto done;
        }
        for (const unsigned char *q = name; *q; q++) if (*q < 32 || *q == 127) goto done;
        printf("%zu\t0x%08" PRIx32 "\t%s\n", i, expected, (const char *)name);
    }
    if (fgets(line, sizeof line, index) || ferror(index) || ferror(elf) || ferror(stdout)) goto done;
    fprintf(stderr, "%zu resource names verified against CRC32 and index order\n", count);
    rc = 0;
done:
    if (rc) fprintf(stderr, "resource-name extraction failed\n");
    if (elf) fclose(elf);
    if (index) fclose(index);
    free(im.data);
    return rc;
}
