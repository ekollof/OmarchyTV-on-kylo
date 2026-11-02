#define main ui_scan_main
#include "../tools/re/ui-object-scan.c"
#undef main
#include <assert.h>

static void put_word(uint8_t *p,uint32_t n)
{ for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*i)); }
int main(void)
{
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
    unsigned count=0;
    assert(!scan_range(fileno(input),0,0x10000000,bytes,output,&count) && count==2);
    assert(!fseek(output,0,SEEK_SET));char line[2048];
    assert(fgets(line,sizeof line,output) && strstr(line,"candidate=1000fffc class=SettingItemsListBoxRow"));
    assert(strstr(line,"created=1 state=1 window=-10,0,0,0 row_type=9") && strstr(line,"label_font=7"));
    assert(fgets(line,sizeof line,output) && strstr(line,"class=SVGTextBox"));
    assert(candidate(output,0,data+row_at,0x2def)==0); /* one byte short */
    assert(candidate(output,0,data,3)==0 && candidate(output,0,data,bytes)==0);
    /* The extended draw-control inventory must not read past a short box,
     * including one that satisfied the historical 0x224-byte minimum. */
    uint8_t *short_box=calloc(1,0x224);assert(short_box);
    put_word(short_box,0x153b904);
    assert(candidate(output,0,short_box,0x224)==0);
    free(short_box);
    uint8_t *box=calloc(1,0x2b1);assert(box);
    put_word(box,0x153b904);
    box[0x1b0]=2;box[0x1b1]=3;put_word(box+0x22c,6);
    put_word(box+0x2a8,UINT32_MAX-4);box[0x2b0]=1;
    assert(candidate(output,0,box,0x2b0)==0);
    assert(!fseek(output,0,SEEK_END));long offset=ftell(output);assert(offset>=0);
    assert(candidate(output,0,box,0x2b1)==1);
    assert(!fseek(output,offset,SEEK_SET) && fgets(line,sizeof line,output));
    assert(strstr(line,"flags1=3 flags2=2 override=6 slide=1 slidermargin=-5"));
    free(box);
    assert(!fclose(input) && !fclose(output));free(data);
    char pid[32];snprintf(pid,sizeof pid,"%ld",(long)getpid());
    char *args[]={"ui-object-scan",pid,NULL};
    assert(ui_scan_main(2,args)==1); /* refuses this executable, not Aurora */
    puts("ok UI object scanner: split-chunk row/embedded textbox, signed fields, truncated/unknown candidate, wrong executable");
    return 0;
}
