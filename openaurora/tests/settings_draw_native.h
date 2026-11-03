/* Included by the offline ARM harness after its checked stub installers. */
#include "settings_draw_fixture.h"
static struct sd_fixture *sd_native;
static _Alignas(uint32_t) unsigned char sd_list[0x300],sd_row[8],sd_item[0xa4];
static uint32_t sd_vector[3],sd_array[1],sd_vtable[25];
struct sd_native_value { uint32_t id;struct sd_text text; };
static struct sd_native_value sd_values[3],sd_values_copy[3];
_Static_assert(sizeof(struct sd_native_value)==12,"shipped value stride");
static void sd_native_emit(enum oa_setting_draw_action action,uint32_t a,uint32_t b,const void *t,const void *t2,const void *image)
{ struct oa_setting_draw_event e={action,a,b,t,t2,image};sd_run(sd_native,sd_row,&e); }
#define SD_WORD(name,action) static void name(void *row,uint32_t v) \
{ assert(row==sd_row);sd_native_emit(action,v,0,NULL,NULL,NULL); }
SD_WORD(sdn_id,OA_SD_ROW_ID) SD_WORD(sdn_progress_mode,OA_SD_PROGRESS_MODE)
SD_WORD(sdn_mixed,OA_SD_MIXED_MODE) SD_WORD(sdn_ball,OA_SD_BALL)
SD_WORD(sdn_long,OA_SD_LONG_BUTTON) SD_WORD(sdn_type,OA_SD_TYPE)
SD_WORD(sdn_active,OA_SD_ACTIVE) SD_WORD(sdn_visible,OA_SD_IMAGE_VISIBLE)
SD_WORD(sdn_selectable,OA_SD_SELECTABLE) SD_WORD(sdn_service,OA_SD_SERVICE_LOGO)
SD_WORD(sdn_glyph,OA_SD_GLYPH) SD_WORD(sdn_selected,OA_SD_SELECTED_ID)
SD_WORD(sdn_selection_image,OA_SD_SELECTION_IMAGE) SD_WORD(sdn_slide,OA_SD_SLIDE)
SD_WORD(sdn_progress,OA_SD_PROGRESS) SD_WORD(sdn_numeric,OA_SD_NUMERIC)
SD_WORD(sdn_checked,OA_SD_CHECKED)
#undef SD_WORD
#define SD_TEXT(name,action) static void name(void *row,uint32_t text) \
{ assert(row==sd_row);sd_native_emit(action,0,0,(const void *)(uintptr_t)text,NULL,NULL); }
SD_TEXT(sdn_label,OA_SD_LABEL) SD_TEXT(sdn_button,OA_SD_BUTTON_TEXT)
SD_TEXT(sdn_url,OA_SD_IMAGE_URL) SD_TEXT(sdn_text,OA_SD_TEXT)
SD_TEXT(sdn_date,OA_SD_DATE) SD_TEXT(sdn_time,OA_SD_TIME)
SD_TEXT(sdn_duration,OA_SD_DURATION) SD_TEXT(sdn_password,OA_SD_PASSWORD)
SD_TEXT(sdn_ip,OA_SD_IP)
#undef SD_TEXT
static void sdn_additional(void *row,uint32_t image)
{ assert(row==sd_row);sd_native_emit(OA_SD_ADDITIONAL,0,0,NULL,NULL,(const void *)(uintptr_t)image); }
static void sdn_image(void *row,uint32_t image)
{ assert(row==sd_row);sd_native_emit(OA_SD_IMAGE_DETAIL,0,0,NULL,NULL,(const void *)(uintptr_t)image); }
static unsigned sdn_clear(void *row)
{ assert(row==sd_row);sd_native_emit(OA_SD_CLEAR_ITEMS,0,0,NULL,NULL,NULL);return 0; }
static void sdn_range(void *row,uint32_t a,uint32_t b)
{ assert(row==sd_row);sd_native_emit(OA_SD_RANGE,a,b,NULL,NULL,NULL); }
static void sdn_images(void *row,uint32_t a,uint32_t b)
{ assert(row==sd_row);sd_native_emit(OA_SD_BAR_IMAGES,a,b,NULL,NULL,NULL); }
static void sdn_minmax(void *row,uint32_t a,uint32_t b)
{ assert(row==sd_row);sd_native_emit(OA_SD_MINMAX_TEXT,0,0,(const void *)(uintptr_t)a,(const void *)(uintptr_t)b,NULL); }
static void sdn_path(void *row,uint32_t a,uint32_t b)
{ assert(row==sd_row);sd_native_emit(OA_SD_GLYPH_PATH,0,0,(const void *)(uintptr_t)a,(const void *)(uintptr_t)b,NULL); }
static void sdn_append_item(void *row,uint32_t a,uint32_t b)
{ assert(row==sd_row);sd_native_emit(OA_SD_APPEND_ITEM,a,0,(const void *)(uintptr_t)b,NULL,NULL); }
static void sdn_new(void *target,uint32_t source)
{ assert(sd_construct(sd_native,target,(const void *)(uintptr_t)source)); }
static void sdn_new_chars(void *target,uint32_t source)
{ assert(source && !*(const char *)(uintptr_t)source);assert(sd_construct(sd_native,target,NULL)); }
static unsigned sdn_new_empty(void *target)
{ assert(sd_construct(sd_native,target,NULL));return 0; }
static void sdn_assign(void *target,uint32_t source) { sd_assign(sd_native,target,(const void *)(uintptr_t)source); }
static void sdn_append(void *target,uint32_t value) { sd_append(sd_native,target,value); }
static unsigned sdn_empty(void *target) { return (unsigned)sd_empty(sd_native,target); }
static unsigned sdn_equal(void *target,int32_t literal)
{ assert(literal && !*(const char *)(uintptr_t)(uint32_t)literal);return (unsigned)sd_equal(sd_native,target); }
static unsigned sdn_free(void *target) { sd_free(sd_native,target);return 0; }
static unsigned sdn_resources(void *framework) { assert(framework==sd_list);return (uint32_t)(uintptr_t)sd_native; }
static void sdn_lookup(void *resources,uint32_t name,uint32_t target)
{ assert(resources==sd_native && !strcmp((const char *)(uintptr_t)name,"OK"));sd_lookup(sd_native,(void *)(uintptr_t)target); }
static void sdn_vector_copy(void *target,uint32_t source)
{
    uint32_t *v=(uint32_t *)(uintptr_t)source;uint32_t n=(v[1]-v[0])/12;
    assert(n<=3 && !sd_native->vector_live);sd_native->vector_live=1;
    sd_record(sd_native,SD_COPY,n,0,0,0,NULL);
    memcpy(sd_values_copy,(const void *)(uintptr_t)v[0],n*12);
    uint32_t copy[3]={(uint32_t)(uintptr_t)sd_values_copy,(uint32_t)(uintptr_t)(sd_values_copy+n),0};
    memcpy(target,copy,sizeof copy);
}
static unsigned sdn_vector_free(void *target)
{
    uint32_t v[3];memcpy(v,target,sizeof v);assert(sd_native->vector_live);
    for(unsigned i=1;i<16;++i)assert(!sd_native->pool[i].live);
    sd_record(sd_native,SD_DROP,(v[1]-v[0])/12,0,0,0,NULL);sd_native->vector_live=0;
    memset(sd_values_copy,0xdd,sizeof sd_values_copy);return 0;
}
typedef void (*sd_triple)(void *,uint32_t,uint32_t);
static void sd_triple_stub(void *mapping,uint32_t base,uint32_t address,sd_triple fn)
{ uint32_t target;memcpy(&target,&fn,4);raw_stub(mapping,base,address,target); }
static void sd_put(unsigned char *object,size_t offset,uint32_t value) { memcpy(object+offset,&value,4); }
static void check_settings_draw(const unsigned char *elf,size_t bytes)
{
    const uint32_t bases[]={0xd50000,0xd52000,0xd54000,0xc86000,0xc88000,0xd28000,0x113b000,0x114b000};
    const size_t sizes[]={8192,8192,4096,8192,4096,4096,4096,4096};unsigned char *p[8];
    for(unsigned i=0;i<8;++i)p[i]=page(bases[i],sizes[i]);
    size_t off=file_offset(elf,bytes,0xd50f60,2448);
    assert(crc32(elf+off,2448)==UINT32_C(0xc91978ad));
    memcpy(stub_destination(p[0],bases[0],0xd50f60,2448),elf+off,2448);
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;sd_put(p[0],0x1260,pointer);
    static const char empty[]="",ok[]="OK";
    /* Literal bytes and IDs are asserted against the ELF before relocation. */
    uint32_t literal;memcpy(&literal,elf+file_offset(elf,bytes,0xd51264,4),4);
    assert(literal==0x12f5cda && !elf[file_offset(elf,bytes,literal,1)]);
    memcpy(&literal,elf+file_offset(elf,bytes,0xd51268,4),4);
    assert(literal==0x151c06c && !memcmp(elf+file_offset(elf,bytes,literal,3),"OK",3));
    sd_put(p[0],0x1264,(uint32_t)(uintptr_t)empty);sd_put(p[0],0x1268,(uint32_t)(uintptr_t)ok);
    /* The embedded table and inverse-three literal remain the shipped bytes. */
    const struct {unsigned page;uint32_t address;pair_fn fn;} pairs[]={
        {0,0xd51dc8,sdn_id},{0,0xd51e00,sdn_type},
        {1,0xd52bd8,sdn_progress_mode},{1,0xd52c10,sdn_mixed},{1,0xd52ba0,sdn_ball},
        {1,0xd52b68,sdn_long},{1,0xd52498,sdn_label},{1,0xd524fc,sdn_button},
        {1,0xd52554,sdn_visible},{1,0xd528b8,sdn_additional},{1,0xd528f0,sdn_url},
        {1,0xd52a88,sdn_service},{1,0xd53bdc,sdn_image},{1,0xd5278c,sdn_glyph},
        {1,0xd52c98,sdn_selected},{2,0xd54374,sdn_selection_image},
        {1,0xd52efc,sdn_slide},{1,0xd52e40,sdn_progress},{1,0xd52dec,sdn_numeric},
        {1,0xd52d98,sdn_text},{1,0xd5300c,sdn_date},{1,0xd530a4,sdn_time},
        {1,0xd530f8,sdn_duration},{1,0xd531a0,sdn_password},{1,0xd5314c,sdn_ip},
        {3,0xc867f4,sdn_new},{3,0xc86744,sdn_new_chars},{3,0xc86b00,sdn_assign},
        {4,0xc8865c,sdn_append},{5,0xd28338,sdn_vector_copy}
    };
    for(unsigned i=0;i<sizeof pairs/sizeof *pairs;++i)pair_stub(p[pairs[i].page],bases[pairs[i].page],pairs[i].address,pairs[i].fn);
    const struct {unsigned page;uint32_t address;sd_triple fn;} triples[]={
        {1,0xd53330,sdn_range},{1,0xd53424,sdn_images},{1,0xd533a4,sdn_minmax},
        {1,0xd52960,sdn_path},{1,0xd52d60,sdn_append_item},{7,0x114bd24,sdn_lookup}
    };
    for(unsigned i=0;i<sizeof triples/sizeof *triples;++i)sd_triple_stub(p[triples[i].page],bases[triples[i].page],triples[i].address,triples[i].fn);
    thumb_stub(p[1],bases[1],0xd52cf0,sdn_clear);
    thumb_stub(p[3],bases[3],0xc8648c,sdn_new_empty);thumb_stub(p[3],bases[3],0xc86554,sdn_free);
    thumb_stub(p[3],bases[3],0xc86c60,sdn_empty);
    raw_stub(p[4],bases[4],0xc88274,predicate_bits(sdn_equal));
    thumb_stub(p[5],bases[5],0xd282f8,sdn_vector_free);thumb_stub(p[6],bases[6],0x113ba24,sdn_resources);
    sd_vtable[0x34/4]=pair_bits(sdn_selectable);sd_vtable[0x40/4]=pair_bits(sdn_active);sd_vtable[0x60/4]=pair_bits(sdn_checked);
    sd_put(sd_row,0,(uint32_t)(uintptr_t)sd_vtable);sd_put(sd_list,4,(uint32_t)(uintptr_t)sd_list);
    sd_put(sd_list,0x28c,(uint32_t)(uintptr_t)sd_vector);sd_array[0]=(uint32_t)(uintptr_t)sd_item;
    sd_vector[0]=(uint32_t)(uintptr_t)sd_array;sd_vector[1]=(uint32_t)(uintptr_t)(sd_array+1);
    for(unsigned i=0;i<8;++i)executable(p[i],sizes[i]);
    void (*draw)(void *,int32_t,void *)=NULL;pointer=0xd50f61;memcpy(&draw,&pointer,4);
    unsigned compared=0;
    for(unsigned type_index=0;type_index<36;++type_index)for(unsigned flags=0;flags<16;++flags)for(unsigned variant=0;variant<32;++variant) {
        uint32_t type=type_index<32?type_index:(type_index==32?UINT32_MAX:type_index==33?UINT32_C(0x80000000):type_index==34?UINT32_C(0x10000009):UINT32_C(0xfffffff9));
        struct sd_text label={variant&1?0:17,0},path={variant&2?18:0,0},url={variant&4?19:0,0};
        struct oa_setting_value values[3];
        for(unsigned i=0;i<3;++i) { sd_values[i]=(struct sd_native_value){UINT32_MAX-i,{variant&8?0:21+i,0}};values[i]=(struct oa_setting_value){sd_values[i].id,&sd_values[i].text}; }
        struct oa_setting_item item={.id=UINT32_C(0x8abcdef0),.type=type,.value=UINT32_C(0x80000000)+variant,
            .minimum=UINT32_MAX-variant,.maximum=INT32_MAX-variant,.min_image=31,.max_image=42,
            .service_logo=variant&16?7:UINT32_MAX,.glyph=UINT32_C(0x87654321),.active=(uint8_t)(variant&1?255:0),.checked=(uint8_t)(variant&2?255:1),
            .label=&label,.glyph_path=&path,.image_url=&url,.image={variant&8?3:UINT32_MAX,sd_item+0x84},
            .additional={variant&16?5:UINT32_MAX,sd_item+0x94},.values=values,.value_count=variant%4};
        struct oa_setting_draw_style style={flags&1,(flags>>1)&1,(flags>>2)&1,(flags>>3)&1};
        struct sd_fixture vendor={.row=sd_row},ref={.row=sd_row};struct oa_setting_draw_ops ops=sd_ops(&ref);
        memset(sd_item,0,sizeof sd_item);sd_put(sd_item,0,item.id);memcpy(sd_item+4,&label,8);sd_put(sd_item,0x10,type);
        sd_put(sd_item,0x14,(uint32_t)(uintptr_t)sd_values);sd_put(sd_item,0x18,(uint32_t)(uintptr_t)(sd_values+item.value_count));
        sd_put(sd_item,0x20,item.value);sd_put(sd_item,0x24,item.minimum);sd_put(sd_item,0x28,item.maximum);
        sd_item[0x2c]=item.active;sd_item[0x2d]=item.checked;sd_put(sd_item,0x30,item.min_image);sd_put(sd_item,0x34,item.max_image);
        memcpy(sd_item+0x38,&path,8);memcpy(sd_item+0x48,&url,8);sd_put(sd_item,0x78,item.service_logo);sd_put(sd_item,0x7c,item.glyph);
        sd_put(sd_item,0x84,item.image.id);sd_put(sd_item,0x94,item.additional.id);
        sd_list[0x290]=style.no_default;sd_list[0x291]=style.ball;sd_list[0x292]=style.progress;sd_list[0x293]=style.mixed;
        unsigned char before[sizeof sd_item];memcpy(before,sd_item,sizeof before);
        sd_native=&vendor;{ void (*volatile call)(void *,int32_t,void *)=draw;call(sd_list,0,sd_row); }
        struct oa_setting_item *items[]={&item};assert(!oa_settings_draw_row(&ops,items,1,0,sd_row,&style));
        uint32_t stored;memcpy(&stored,sd_item+0x10,4);sd_put(before,0x10,item.type);
        if(stored!=item.type || memcmp(before,sd_item,sizeof before) || vendor.count!=ref.count || memcmp(vendor.trace,ref.trace,ref.count*sizeof *ref.trace)) {
            fprintf(stderr,"settings draw mismatch type=%x flags=%x variant=%u counts=%u/%u types=%x/%x\n",type,flags,variant,vendor.count,ref.count,stored,item.type);
            for(unsigned j=0;j<vendor.count || j<ref.count;++j)fprintf(stderr,"%u: native=%u/%x/%x/%x/%x ref=%u/%x/%x/%x/%x\n",j,
                vendor.trace[j].id,vendor.trace[j].a,vendor.trace[j].b,vendor.trace[j].c,vendor.trace[j].d,
                ref.trace[j].id,ref.trace[j].a,ref.trace[j].b,ref.trace[j].c,ref.trace[j].d);
            exit(1);
        }
        sd_no_leak(&vendor);sd_no_leak(&ref);++compared;
    }
    const struct {int32_t count,index;} bounds[]={
        {0,0},{0,-1},{0,1},{0,INT32_MAX},{1,-1},{1,1},{1,2},{1,INT32_MIN},{1,INT32_MAX}
    };
    for(unsigned i=0;i<sizeof bounds/sizeof *bounds;++i) {
        struct sd_fixture vendor={.row=sd_row},ref={.row=sd_row};struct oa_setting_draw_ops ops=sd_ops(&ref);
        struct oa_setting_draw_style style={0};unsigned char before[sizeof sd_item];memcpy(before,sd_item,sizeof before);
        sd_vector[1]=sd_vector[0]+4u*(uint32_t)bounds[i].count;sd_native=&vendor;
        { void (*volatile call)(void *,int32_t,void *)=draw;call(sd_list,bounds[i].index,sd_row); }
        assert(!oa_settings_draw_row(&ops,NULL,bounds[i].count,bounds[i].index,sd_row,&style));
        assert(!vendor.count && !ref.count && !memcmp(before,sd_item,sizeof before));++compared;
    }
    sd_native=NULL;for(unsigned i=0;i<8;++i)assert(!munmap(p[i],sizes[i]));
    printf("ok offline shipped ARM settings draw: %u native/C comparisons, all types/style writeback/owned temporary order match\n",compared);
}
