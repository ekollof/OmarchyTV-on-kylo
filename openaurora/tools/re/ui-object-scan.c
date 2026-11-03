/* Read-only candidate inventory for Aurora MD5 64c252340d3af6c5d66072611b7bad7a.
 * No ptrace, signals, device fds, input reads or target function calls.
 * Scan private [heap] maps; follow key pointers only within those maps or
 * read-only private mappings of the verified Aurora executable.
 * Vtable matches are NON-ATOMIC CANDIDATES, not proof of active UI/lifetime.
 * Firmware field evidence: UI-LIVE-OBJECTS / UI-TEXT-VIEWPORT / UI-TEXTBOX-COMPOSE.
 */
#define _FILE_OFFSET_BITS 64
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { BLOCK=65536, LOOKAHEAD=0x3000 };
enum { MAX_REGIONS=128 };
struct region { uint64_t low,high,offset; int heap; };
struct reader { int fd; const struct region *regions; size_t count; };
#define SCAN_LIMIT (UINT64_C(256)*1024*1024)
/* size is the minimum readable span for this inventory, not sizeof(class). */
struct identity { uint32_t vptr; size_t size; const char *name; unsigned kind; };
static const struct identity identities[]={
    {0x014dbe2c,0x2df0,"SettingItemsListBoxRow",1},
    {0x014dbd74,0x240,"SettingItemsListBox",2},
    {0x0153b904,0x2b1,"SVGTextBox",3},
    {0x014e87a4,0x94,"PictureSettingsDialog",0},
    {0x014dbeac,0x94,"SettingsDialog",0}
};
static uint32_t word(const uint8_t *p)
{ return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static int32_t signed_word(const uint8_t *p)
{ uint32_t n=word(p);return (int32_t)((int64_t)n-(n&UINT32_C(0x80000000)?INT64_C(4294967296):0)); }
static const struct identity *identity(uint32_t vptr)
{
    for(size_t i=0;i<sizeof identities/sizeof *identities;++i)
        if(identities[i].vptr==vptr)return &identities[i];
    return NULL;
}
static void rectangle(FILE *out,const char *name,const uint8_t *p)
{
    fprintf(out," %s=%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32,
            name,signed_word(p),signed_word(p+4),signed_word(p+8),signed_word(p+12));
}
static int read_at(int fd,uint8_t *buf,size_t size,uint64_t address);
static void key_text(FILE *out,const struct reader *reader,uint32_t address)
{
    unsigned char text[40];size_t length=0;
    const char *status="unterminated";
    for(size_t i=0;i<=sizeof text;++i) {
        uint64_t at=(uint64_t)address+i;const struct region *region=NULL;
        if(reader)for(size_t j=0;j<reader->count;++j)
            if(at>=reader->regions[j].low && at<reader->regions[j].high) {
                region=reader->regions+j;break;
            }
        if(!region){status="outside-readable-maps";break;}
        unsigned char c;
        if(read_at(reader->fd,&c,1,region->offset+at-region->low)) {
            status="unreadable";break;
        }
        if(!c) {
            fputs(" key=\"",out);
            for(size_t j=0;j<length;++j) {
                if(text[j]=='"' || text[j]=='\\')fputc('\\',out);
                fputc(text[j],out);
            }
            fputc('"',out);return;
        }
        if(c<32 || c>126){status="nonprintable";break;}
        if(i==sizeof text)break;
        text[length++]=c;
    }
    fprintf(out," keyptr=%08" PRIx32 " key_status=%s",address,status);
}
static int candidate(FILE *out,const struct reader *reader,uint32_t address,const uint8_t *p,size_t size)
{
    if(size<4)return 0;
    const struct identity *id=identity(word(p));
    if(!id || size<id->size)return 0;
    fprintf(out,"candidate=%08" PRIx32 " class=%s created=%u state=%" PRIu32,
            address,id->name,p[0x64],word(p+0x6c));
    rectangle(out,"window",p+0x28);
    if(id->kind==1) {
        fprintf(out," row_type=%" PRIu32 " label_vptr=%08" PRIx32,word(p+0x2dec),word(p+0x104));
        rectangle(out,"label_window",p+0x104+0x28);
        rectangle(out,"label_text",p+0x104+0x1f4);
        fprintf(out," label_font=%" PRIu32 " label_lines=%" PRId32,
                word(p+0x104+0x21c),signed_word(p+0x104+0x1b4));
    } else if(id->kind==2) {
        fprintf(out," count=%" PRId32 " step=%" PRId32 " row_height=%" PRId32
                    " insets=%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32
                    " row_items=%" PRIu32,
                signed_word(p+0x1f0),signed_word(p+0x1f4),signed_word(p+0x1f8),
                signed_word(p+0x204),signed_word(p+0x1fc),signed_word(p+0x208),signed_word(p+0x200),word(p+0xe4));
    } else if(id->kind==3) {
        rectangle(out,"text",p+0x1f4);
        fprintf(out," font=%" PRIu32 " lines=%" PRId32 " font_type=%" PRIu32 " alignment=%" PRIu32
                " flags1=%u flags2=%u override=%" PRIu32 " slide=%u slidermargin=%" PRId32,
                word(p+0x21c),signed_word(p+0x1b4),word(p+0x1c4),word(p+0x220),
                p[0x1b1],p[0x1b0],word(p+0x22c),p[0x2b0],signed_word(p+0x2a8));
        /* +ac is a lookup-key pointer, not necessarily current text: the
         * UString setter does not clear it. Always retain raw +a4 metadata. */
        uint32_t key=word(p+0xac);
        if(key)key_text(out,reader,key);else fputs(" key=(none)",out);
        fprintf(out," ustring=%02x%02x%02x%02x%02x%02x%02x%02x",
                p[0xa4],p[0xa5],p[0xa6],p[0xa7],p[0xa8],p[0xa9],p[0xaa],p[0xab]);
    }
    fputc('\n',out);
    return 1;
}
static int read_at(int fd,uint8_t *buf,size_t size,uint64_t address)
{
    size_t at=0;
    while(at<size) {
        ssize_t n=pread(fd,buf+at,size-at,(off_t)(address+at));
        if(n<0 && errno==EINTR)continue;
        if(n<=0)return -1;
        at+=(size_t)n;
    }
    return 0;
}
static int scan_range(const struct reader *reader,uint64_t offset,uint32_t address,size_t size,FILE *out,unsigned *count)
{
    uint8_t *buf=malloc(BLOCK+LOOKAHEAD);
    if(!buf)return -1;
    for(size_t at=0;at<size;) {
        size_t remaining=size-at,scan=remaining<BLOCK?remaining:BLOCK;
        size_t read=remaining<BLOCK+LOOKAHEAD?remaining:BLOCK+LOOKAHEAD;
        if(read_at(reader->fd,buf,read,offset+at)){free(buf);return -1;}
        for(size_t i=0;i+4<=scan;i+=4) {
            if(candidate(out,reader,address+(uint32_t)(at+i),buf+i,read-i))++*count;
        }
        at+=scan;
    }
    free(buf);
    return 0;
}
static int parse_number(const char *s,unsigned base,uint32_t *out)
{
    if(!s || !*s || *s=='-' || *s=='+')return -1;
    char *end;errno=0;unsigned long long n=strtoull(s,&end,(int)base);
    if(errno || *end || n>UINT32_MAX)return -1;
    *out=(uint32_t)n;return 0;
}
static int snapshot(const char *base,const char *path)
{
    uint32_t address;if(parse_number(base,16,&address))return 1;
    int fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);if(fd<0)return 1;
    int rc=1;struct stat st;
    if(fstat(fd,&st) || !S_ISREG(st.st_mode))goto done;
    off_t size=st.st_size;
    if(size<=0 || (uint64_t)size>SCAN_LIMIT || (uint64_t)size>UINT64_C(0x100000000)-address)goto done;
    const struct region region={address,(uint64_t)address+(uint64_t)size,0,1};
    const struct reader reader={fd,&region,1};
    unsigned count=0;
    if(!scan_range(&reader,0,address,(size_t)size,stdout,&count)) {
        printf("snapshot_bytes=%" PRIu64 " candidates=%u\n",(uint64_t)size,count);rc=0;
    }
done:
    if(close(fd))rc=1;
    return rc || ferror(stdout)?1:0;
}
static int map_region(const char *line,struct region *out)
{
    uint64_t low,high,file_offset;unsigned major,minor;
    unsigned long long inode;int used=0;char perms[5];
    if(sscanf(line,"%" SCNx64 "-%" SCNx64 " %4s %" SCNx64 " %x:%x %llu %n",
              &low,&high,perms,&file_offset,&major,&minor,&inode,&used)!=7 || !used)return 0;
    const char *path=line+used;
    int heap=!strcmp(perms,"rw-p") && !file_offset && !major && !minor && !inode &&
             !strncmp(path,"[heap]",6) && (path[6]=='\n' || !path[6]);
    const char *aurora="/vendor/com.nvt.idtv/aurora.elf";
    size_t n=strlen(aurora);
    int executable=(!strcmp(perms,"r--p") || !strcmp(perms,"r-xp")) && inode &&
                   !strncmp(path,aurora,n) && (path[n]=='\n' || !path[n]);
    if(!heap && !executable)return 0;
    if(high<=low || high>UINT64_C(0x100000000))return -1;
    *out=(struct region){low,high,low,heap};return 1;
}
int main(int argc,char **argv)
{
    if(argc==4 && !strcmp(argv[1],"--snapshot"))return snapshot(argv[2],argv[3]);
    uint32_t pid;
    if(argc!=2 || parse_number(argv[1],10,&pid) || !pid || pid>INT_MAX) {
        fprintf(stderr,"usage: %s PID | --snapshot HEX_BASE FILE\n",argv[0]);return 2;
    }
    char path[96],exe[256];
    snprintf(path,sizeof path,"/proc/%" PRIu32 "/exe",pid);
    ssize_t n=readlink(path,exe,sizeof exe-1);
    if(n<0)return 1;
    exe[n]=0;
    if(strcmp(exe,"/vendor/com.nvt.idtv/aurora.elf")) {
        fputs("refusing process with unexpected executable\n",stderr);return 1;
    }
    snprintf(path,sizeof path,"/proc/%" PRIu32 "/maps",pid);
    FILE *maps=fopen(path,"r");if(!maps)return 1;
    snprintf(path,sizeof path,"/proc/%" PRIu32 "/mem",pid);
    int fd=open(path,O_RDONLY|O_CLOEXEC);
    if(fd<0){fclose(maps);perror("proc memory");return 1;}
    char line[1024];unsigned count=0,ranges=0,heap_count=0;
    uint64_t total=0,eligible_total=0;
    struct region regions[MAX_REGIONS];size_t region_count=0;int rc=0;
    /* Inventory all eligible mappings before following any pointer. */
    while(fgets(line,sizeof line,maps)) {
        struct region region;int result=map_region(line,&region);
        if(result<0 || (result && region_count==MAX_REGIONS)){rc=1;break;}
        if(!result)continue;
        if(region.heap) {
            if(region.high-region.low>SCAN_LIMIT-eligible_total){rc=1;break;}
            eligible_total+=region.high-region.low;++heap_count;
        }
        regions[region_count++]=region;
    }
    if(ferror(maps) || !heap_count)rc=1;
    if(fclose(maps))rc=1;
    const struct reader reader={fd,regions,region_count};
    puts("NONATOMIC heap candidates; key reads limited to private heap/Aurora read-only maps; no active-window inference");
    if(!rc)for(size_t i=0;i<region_count;++i) {
        const struct region *region=regions+i;
        if(!region->heap)continue;
        if(scan_range(&reader,region->offset,(uint32_t)region->low,
                      (size_t)(region->high-region->low),stdout,&count)) {rc=1;break;}
        total+=region->high-region->low;++ranges;
    }
    if(close(fd))rc=1;
    printf("heap_ranges=%u scanned_bytes=%" PRIu64 " candidates=%u complete=%d\n",ranges,total,count,!rc);
    return rc || ferror(stdout)?1:0;
}
