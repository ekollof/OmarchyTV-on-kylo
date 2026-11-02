#include "openaurora/svg_pack.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

struct entry { uint32_t hash; size_t offset, size; };
struct oa_svg_pack { unsigned count; struct entry *entries; uint8_t *data; };

static int read_regular(const char *path, size_t limit, uint8_t **out, size_t *size)
{
    int fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
    if (fd<0) return errno;
    struct stat st;
    int rc=0;
    uint8_t *bytes=NULL;
    if (fstat(fd,&st)) { rc=errno; goto done; }
    if (!S_ISREG(st.st_mode) || st.st_size<=0 || (uintmax_t)st.st_size>limit) {
        rc=EINVAL; goto done;
    }
    size_t n=(size_t)st.st_size, at=0;
    bytes=malloc(n+1);
    if (!bytes) { rc=ENOMEM; goto done; }
    /* Read one extra byte to reject growth/extra data, without mapping a file
     * which a modder can truncate underneath the snapshot. */
    while (at<n+1) {
        ssize_t got=read(fd,bytes+at,n+1-at);
        if (got<0) { if (errno==EINTR) continue; rc=errno; goto done; }
        if (!got) break;
        at+=(size_t)got;
    }
    if (at!=n) { rc=EINVAL; goto done; }
    bytes[n]=0;
    *size=n;
done:
    if (close(fd) && !rc) rc=errno;
    if (rc) free(bytes);
    else *out=bytes;
    return rc;
}
static int whitespace(unsigned char c)
{ return c==' ' || c=='\t' || c=='\n' || c=='\r' || c=='\v' || c=='\f'; }
static void spaces(const uint8_t **p) { while (whitespace(**p)) ++*p; }
static int number(const uint8_t **cursor, unsigned base, uintmax_t limit, uintmax_t *out)
{
    const uint8_t *p=*cursor;
    spaces(&p);
    if (base==16 && p[0]=='0' && (p[1]=='x' || p[1]=='X')) p+=2;
    unsigned digits=0;
    uintmax_t value=0;
    while (*p && !whitespace(*p)) {
        unsigned d=*p>='0' && *p<='9' ? (unsigned)(*p-'0') :
                   *p>='a' && *p<='f' ? *p-'a'+10u :
                   *p>='A' && *p<='F' ? *p-'A'+10u : base;
        if (d>=base || d>limit || value>(limit-d)/base) return EINVAL;
        value=value*base+d; ++p; ++digits;
    }
    if (!digits) return EINVAL;
    *cursor=p; *out=value;
    return 0;
}
void oa_svg_pack_free(struct oa_svg_pack *p)
{
    if (!p) return;
    free(p->entries); free(p->data); free(p);
}
int oa_svg_pack_load(struct oa_svg_pack **out, const char *index, const char *binary)
{
    if (!out || !index || !*index || !binary || !*binary) return EINVAL;
    uint8_t *text=NULL;
    size_t text_size, data_size;
    struct oa_svg_pack *p=calloc(1,sizeof *p);
    if (!p) return ENOMEM;
    int rc=read_regular(index,4u<<20,&text,&text_size);
    if (rc) goto done;
    /* Embedded NUL must not hide trailing entries or junk. */
    for (size_t i=0;i<text_size;++i) if (!text[i]) { rc=EINVAL; goto done; }
    const uint8_t *cursor=text;
    uintmax_t count;
    if ((rc=number(&cursor,10,65536,&count))) goto done;
    if (!count) { rc=EINVAL; goto done; }
    p->count=(unsigned)count;
    p->entries=calloc(p->count,sizeof *p->entries);
    if (!p->entries) { rc=ENOMEM; goto done; }
    size_t total=0;
    for (unsigned i=0;i<p->count;++i) {
        uintmax_t hash, size;
        if ((rc=number(&cursor,16,UINT32_MAX,&hash)) ||
            (rc=number(&cursor,10,(64u<<20)-total,&size))) goto done;
        if (!size) { rc=EINVAL; goto done; }
        p->entries[i]=(struct entry){(uint32_t)hash,total,(size_t)size};
        total+=(size_t)size;
    }
    spaces(&cursor);
    if (*cursor) { rc=EINVAL; goto done; }
    if ((rc=read_regular(binary,64u<<20,&p->data,&data_size))) goto done;
    if (total!=data_size) { rc=EINVAL; goto done; }
    for (unsigned i=0;i<p->count;++i) {
        struct oa_binsvg view;
        rc=oa_binsvg_init(&view,p->data+p->entries[i].offset,p->entries[i].size);
        if (rc) goto done;
    }
done:
    free(text);
    if (rc) oa_svg_pack_free(p);
    else *out=p;
    return rc;
}
unsigned oa_svg_pack_count(const struct oa_svg_pack *p) { return p?p->count:0; }
int oa_svg_pack_record(const struct oa_svg_pack *p, unsigned id, struct oa_binsvg *out)
{
    if (!p || !out) return EINVAL;
    if (id>=p->count) return ENOENT;
    *out=(struct oa_binsvg){p->data+p->entries[id].offset,p->entries[id].size};
    return 0;
}
int oa_svg_pack_named(const struct oa_svg_pack *p, const char *name, struct oa_binsvg *out)
{
    if (!p || !name || !*name || !out) return EINVAL;
    uint32_t crc=UINT32_MAX;
    for (const unsigned char *s=(const unsigned char *)name;*s;++s) {
        crc^=*s;
        for (unsigned i=0;i<8;++i) crc=(crc>>1)^((crc&1)?UINT32_C(0xedb88320):0);
    }
    crc=~crc;
    for (unsigned i=0;i<p->count;++i)
        if (p->entries[i].hash==crc) return oa_svg_pack_record(p,i,out);
    return ENOENT;
}
