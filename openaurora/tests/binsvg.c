#include "openaurora/binsvg.h"
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct bytes { uint8_t data[4096]; size_t size; };
static void byte(struct bytes *b, unsigned x)
{ assert(b->size<sizeof b->data); b->data[b->size++]=(uint8_t)x; }
static void word(struct bytes *b, int x)
{ byte(b,(unsigned)x); byte(b,(unsigned)x>>8); }
static void str(struct bytes *b, const char *s)
{ do { byte(b,(unsigned char)*s); } while (*s++); }
static void rect(struct bytes *b, const char *id, int x)
{
    byte(b,1); str(b,id);
    word(b,x); word(b,-4); word(b,-7); word(b,13); word(b,-2); byte(b,255);
    str(b,"#010203"); str(b,"none"); str(b,""); str(b,"link"); word(b,-123);
}
static void group(struct bytes *b, const char *id) { byte(b,8); str(b,id); }
static void synthetic(void)
{
    struct bytes b={0};
    rect(&b,"same",99);
    group(&b,"parent"); group(&b,"nested"); rect(&b,"same",-3); byte(&b,9);
    rect(&b,"after",22); byte(&b,9);
    group(&b,"parent"); rect(&b,"later",33); byte(&b,9);
    rect(&b,"",44);
    struct oa_binsvg v;
    assert(!oa_binsvg_init(&v,b.data,b.size));
    struct oa_binsvg_rect r;
    assert(!oa_binsvg_find_rect(&v,NULL,"same",&r) && r.bounds.left==99);
    assert(!oa_binsvg_find_rect(&v,"parent","same",&r) && r.bounds.left==-3);
    assert(r.bounds.top==-4 && r.bounds.right==-10 && r.bounds.bottom==9);
    assert(r.stroke_width==-2 && r.opacity==-1 && r.begin==-123);
    assert(!strcmp(r.stroke,"#010203") && !strcmp(r.fill,"none"));
    assert(!*r.animation && !strcmp(r.animate_transform,"link"));
    assert(!oa_binsvg_find_rect(&v,"nested","same",&r) && r.bounds.left==-3);
    assert(!oa_binsvg_find_rect(&v,"parent","after",&r) && r.bounds.left==22);
    struct oa_binsvg_rect old=r;
    assert(oa_binsvg_find_rect(&v,"parent","later",&r)==ENOENT);
    assert(!memcmp(&old,&r,sizeof r)); /* no second matching group search */
    assert(oa_binsvg_find_rect(&v,"missing","same",&r)==ENOENT);
    assert(oa_binsvg_find_rect(&v,NULL,"",&r)==EINVAL);
    assert(oa_binsvg_find_rect(&v,"","same",&r)==EINVAL);
    size_t at=0, count=0;
    struct oa_binsvg_element e={0};
    while (!oa_binsvg_next(&v,&at,&e)) { assert(e.size && e.offset<at); ++count; }
    assert(at==b.size && count==11);
    struct oa_binsvg_element saved=e;
    assert(oa_binsvg_next(&v,&at,&e)==ENOENT && at==b.size && !memcmp(&e,&saved,sizeof e));
    ++at;
    assert(oa_binsvg_next(&v,&at,&e)==EINVAL && at==b.size+1);

    /* Every tag has a deliberately nonempty string layout. Exhaust all byte
     * truncations, including the rectangle's final word and animation begin.
     * next() is bounded even before init, which is useful for malformed tests. */
    const unsigned fixed[]={11,11,4,9,11,3,3,3,0};
    const unsigned strings[]={1,4,2,2,2,3,3,3,0};
    for (unsigned tag=0;tag<12;++tag) {
        b=(struct bytes){0}; byte(&b,tag);
        if (tag!=9) {
            str(&b,"id");
            if (tag>=10) {
                byte(&b,2); word(&b,-7);
                for (unsigned i=0;i<3;++i) str(&b,"abc");
                word(&b,-9);
                for (unsigned i=0;i<6;++i) str(&b,"xyz");
            } else {
                for (unsigned i=0;i<fixed[tag];++i) byte(&b,0);
                for (unsigned i=0;i<strings[tag];++i) str(&b,"value");
                if (tag==1) word(&b,-10);
            }
        }
        for (size_t n=0;n<b.size;++n) {
            struct oa_binsvg short_view={b.data,n}; at=0; e=saved;
            assert(oa_binsvg_next(&short_view,&at,&e)==(n?EINVAL:ENOENT));
            assert(!at && !memcmp(&e,&saved,sizeof e));
        }
        struct oa_binsvg one={b.data,b.size}; at=0;
        assert(!oa_binsvg_next(&one,&at,&e) && at==b.size && e.tag==tag);
        assert(tag==9 ? e.id==NULL : !strcmp(e.id,"id"));
        struct oa_binsvg before=v;
        int rc=oa_binsvg_init(&v,b.data,b.size);
        if (tag==8 || tag==9) assert(rc==EINVAL && !memcmp(&v,&before,sizeof v));
        else assert(!rc);
    }
    b.data[0]=12;
    struct oa_binsvg before=v;
    assert(oa_binsvg_init(&v,b.data,b.size)==EINVAL && !memcmp(&before,&v,sizeof v));
    assert(oa_binsvg_init(&v,b.data,0)==EINVAL && !memcmp(&before,&v,sizeof v));
    assert(oa_binsvg_init(&v,NULL,2)==EINVAL);
    puts("ok BinSVG: all tags/truncations, signed rectangles, nested/first group and duplicate lookup, atomic errors");
}
static struct oa_text_rect find(const struct oa_binsvg *v, const char *name)
{
    struct oa_binsvg_rect r;
    assert(!oa_binsvg_find_rect(v,NULL,name,&r));
    return r.bounds;
}
static void compose(const struct oa_binsvg *v)
{
    struct oa_text_rect tl=find(v,"mTopL"), l=find(v,"mRowL"), m=find(v,"mRowM");
    struct oa_text_rect r=find(v,"mRowR"), bl=find(v,"mBotL"), row=find(v,"rowPos");
    struct oa_list_slices slices={&tl,&l,&m,&r,&bl};
    struct oa_list_metrics metrics={.window={0,0,750,826}};
    assert(!oa_list_measure_slices(&metrics,&slices,0));
    assert(metrics.top==16 && metrics.bottom==10 && metrics.vertical_step==70);
    int32_t count=0;
    assert(!oa_list_update_visible_count(&metrics,&count) && count==11);
    struct oa_text_rect middle, outer={0,metrics.top,750,metrics.top+metrics.vertical_step};
    assert(!oa_horizontal_middle_surface(&outer,&l,&r,&middle));
    struct oa_horizontal_scale scale=OA_HORIZONTAL_SCALE_INITIALIZER;
    assert(!oa_horizontal_scale_measure(&scale,&middle,&m));
    assert(!oa_text_rect_stretch_mask(&row,&middle,&m,&scale));
    assert(!oa_list_measure_painted_row(&metrics,&row,&middle,&l,&r));
    assert(metrics.row_height==70 && metrics.item_width==749 && metrics.left==0 && metrics.right==1);
    struct oa_text_rect placed;
    assert(!oa_list_row_rect(&metrics,0,&placed) && !memcmp(&placed,&middle,sizeof placed));
    puts("ok BinSVG: actual477 record -> named slices -> transformed row -> placement (explicit750x826 window)");
}
static void corpus(const char *index_path, const char *pack_path)
{
    FILE *index=fopen(index_path,"r"), *pack=fopen(pack_path,"rb");
    assert(index && pack);
    unsigned records, seen[12]={0}, composed=0;
    assert(fscanf(index,"%u",&records)==1 && records==702);
    size_t total=0;
    for (unsigned i=0;i<records;++i) {
        uint32_t hash; size_t size;
        assert(fscanf(index,"%" SCNx32 " %zu",&hash,&size)==2 && size && size<=64*1024*1024);
        uint8_t *data=malloc(size); assert(data && fread(data,1,size,pack)==size);
        struct oa_binsvg v;
        assert(!oa_binsvg_init(&v,data,size));
        size_t at=0;
        struct oa_binsvg_element e;
        while (!oa_binsvg_next(&v,&at,&e)) {
            ++seen[e.tag];
            if (e.tag==1 && *e.id) {
                struct oa_binsvg_rect r;
                assert(!oa_binsvg_find_rect(&v,NULL,e.id,&r));
            }
        }
        assert(at==size);
        if (hash==0xe8eb982e) { assert(i==477); compose(&v); ++composed; }
        total+=size; free(data);
    }
    assert(fgetc(pack)==EOF && !ferror(pack));
    unsigned nodes=0; for (unsigned i=0;i<12;++i) nodes+=seen[i];
    assert(total==289764 && nodes==11888 && seen[10]+seen[11]==39 && composed==1);
    assert(!fclose(index) && !fclose(pack));
    printf("ok BinSVG corpus: %u records, %u elements, %u rectangles, %u animations, %zu bytes\n",
           records,nodes,seen[1],seen[10]+seen[11],total);
}
int main(int argc, char **argv)
{
    synthetic();
    if (argc==3) corpus(argv[1],argv[2]);
    else assert(argc==1);
    return 0;
}
