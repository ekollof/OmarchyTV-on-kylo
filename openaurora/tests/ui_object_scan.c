#define main ui_scan_main
#include "../tools/re/ui-object-scan.c"
#undef main
#include <assert.h>

static void put_word(uint8_t *p,uint32_t n)
{ for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*i)); }
static void expect_key(const struct reader *reader,uint32_t address,const char *expected)
{
    FILE *out=tmpfile();assert(out);
    key_text(out,reader,address);
    assert(!fseek(out,0,SEEK_SET));char text[256]={0};
    assert(fread(text,1,sizeof text-1,out)==strlen(expected) && !strcmp(text,expected));
    assert(!fclose(out));
}
static void key_fixture(FILE *file,const void *data,size_t size)
{
    assert(!fseek(file,0,SEEK_SET) && fwrite(data,1,size,file)==size && !fflush(file));
    assert(!ftruncate(fileno(file),(off_t)size));
}
static void key_tests(void)
{
    FILE *file=tmpfile();assert(file);
    struct region region={0x10000000,0x10000080,0,1};
    const struct reader reader={fileno(file),&region,1};
    key_fixture(file,"MODE",5);
    expect_key(&reader,0x10000000," key=\"MODE\""); /* virtual base -> file offset0 */
    key_fixture(file,"A\"\\B",5);
    expect_key(&reader,0x10000000," key=\"A\\\"\\\\B\"");
    key_fixture(file,"MODE",4); /* mapped but short file: reject the prefix */
    expect_key(&reader,0x10000000," keyptr=10000000 key_status=unreadable");
    key_fixture(file,"MODE",5);region.high=region.low+4;
    expect_key(&reader,0x10000000," keyptr=10000000 key_status=outside-readable-maps");
    region.high=region.low+128;
    key_fixture(file,"MO\n",4);
    expect_key(&reader,0x10000000," keyptr=10000000 key_status=nonprintable");
    char limit[41];memset(limit,'A',sizeof limit);key_fixture(file,limit,sizeof limit);
    expect_key(&reader,0x10000000," keyptr=10000000 key_status=unterminated");
    limit[40]=0;key_fixture(file,limit,sizeof limit);
    expect_key(&reader,0x10000000," key=\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"");
    expect_key(&reader,0x20000000," keyptr=20000000 key_status=outside-readable-maps");
    key_fixture(file,"A",2);region=(struct region){UINT32_MAX,UINT64_C(0x100000000),0,1};
    expect_key(&reader,UINT32_MAX," keyptr=ffffffff key_status=outside-readable-maps");
    assert(!fclose(file));
    struct region parsed;
    assert(map_region("1000-2000 rw-p 00000000 00:00 0 [heap]\n",&parsed)==1 && parsed.heap);
    assert(map_region("1000-2000 r--p 00001000 08:01 42 /vendor/com.nvt.idtv/aurora.elf\n",&parsed)==1 && !parsed.heap);
    assert(map_region("1000-2000 rw-p 00000000 00:00 0 /dev/fb1\n",&parsed)==0);
    assert(map_region("1000-2000 rw-p 00000000 08:01 42 /vendor/com.nvt.idtv/aurora.elf\n",&parsed)==0);
    assert(map_region("1000-2000 r--p 00000000 08:01 42 /vendor/com.nvt.idtv/aurora.elf.bad\n",&parsed)==0);
    assert(map_region("1000-1000 rw-p 00000000 00:00 0 [heap]\n",&parsed)==-1);
}
int main(void)
{
    key_tests();
    uint32_t number;
    assert(parse_number("-1",10,&number) && parse_number("4294967296",10,&number));
    assert(!parse_number("014dbe2c",16,&number) && number==0x14dbe2c);
    size_t bytes=BLOCK+LOOKAHEAD;
    uint8_t *data=calloc(1,bytes);assert(data);
    size_t row_at=BLOCK-4;
    put_word(data+row_at,0x14dbe2c);data[row_at+0x64]=1;put_word(data+row_at+0x6c,1);
    put_word(data+row_at+0x28,UINT32_MAX-9);put_word(data+row_at+0x2dec,9);
    put_word(data+row_at+0x104,0x153b904);data[row_at+0x104+0x64]=1;
    put_word(data+row_at+0x104+0x21c,7);
    FILE *input=tmpfile(),*output=tmpfile();assert(input && output);
    assert(fwrite(data,1,bytes,input)==bytes && !fflush(input));
    const struct region region={0x10000000,0x10000000+bytes,0,1};
    const struct reader reader={fileno(input),&region,1};
    unsigned count=0;
    assert(!scan_range(&reader,0,0x10000000,bytes,output,&count) && count==2);
    assert(!fseek(output,0,SEEK_SET));char line[2048];
    assert(fgets(line,sizeof line,output) && strstr(line,"candidate=1000fffc class=SettingItemsListBoxRow"));
    assert(strstr(line,"created=1 state=1 window=-10,0,0,0 row_type=9") && strstr(line,"label_font=7"));
    assert(fgets(line,sizeof line,output) && strstr(line,"class=SVGTextBox"));
    assert(candidate(output,NULL,0,data+row_at,0x2def)==0); /* one byte short */
    assert(candidate(output,NULL,0,data,3)==0 && candidate(output,NULL,0,data,bytes)==0);
    /* The extended draw-control inventory must not read past a short box,
     * including one that satisfied the historical 0x224-byte minimum. */
    uint8_t *short_box=calloc(1,0x224);assert(short_box);
    put_word(short_box,0x153b904);
    assert(candidate(output,NULL,0,short_box,0x224)==0);
    free(short_box);
    uint8_t *box=calloc(1,0x2b1);assert(box);
    put_word(box,0x153b904);
    box[0x1b0]=2;box[0x1b1]=3;put_word(box+0x22c,6);
    put_word(box+0x2a8,UINT32_MAX-4);box[0x2b0]=1;
    assert(candidate(output,NULL,0,box,0x2b0)==0);
    assert(!fseek(output,0,SEEK_END));long offset=ftell(output);assert(offset>=0);
    assert(candidate(output,NULL,0,box,0x2b1)==1);
    assert(!fseek(output,offset,SEEK_SET) && fgets(line,sizeof line,output));
    assert(strstr(line,"flags1=3 flags2=2 override=6 slide=1 slidermargin=-5"));
    assert(strstr(line,"key=(none) ustring=0000000000000000"));
    /* A recorded lookup key can survive later UString assignment; never hide
     * current raw UString metadata merely because the key pointer is nonzero. */
    put_word(box+0xac,0x20000000);put_word(box+0xa4,12);
    assert(!fseek(output,0,SEEK_END));offset=ftell(output);assert(offset>=0);
    assert(candidate(output,&reader,0,box,0x2b1)==1);
    assert(!fseek(output,offset,SEEK_SET) && fgets(line,sizeof line,output));
    assert(strstr(line,"key_status=outside-readable-maps ustring=0c00000000000000"));
    free(box);
    assert(!fclose(input) && !fclose(output));free(data);
    char pid[32];snprintf(pid,sizeof pid,"%ld",(long)getpid());
    char *args[]={"ui-object-scan",pid,NULL};
    assert(ui_scan_main(2,args)==1); /* refuses this executable, not Aurora */
    puts("ok UI object scanner: geometry/spans, mapped key bounds, snapshot translation, complete/escaped keys, raw UString retention, wrong executable");
    return 0;
}
