#include "openaurora/svg_list_widgets.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
struct nfixture { void *row,*entry;uint32_t value;unsigned count;struct {unsigned id;uint32_t a;} trace[4]; };
static struct nfixture *current_n;
static void nemit(unsigned id,uint32_t a)
{ assert(current_n->count<4);current_n->trace[current_n->count].id=id;current_n->trace[current_n->count].a=a;++current_n->count; }
enum {N_ENTRY,N_GET};
static void *nq_entry(void *row) { nemit(N_ENTRY,(uint32_t)(uintptr_t)row);return current_n->entry; }
static uint32_t nq_get(void *entry) { nemit(N_GET,(uint32_t)(uintptr_t)entry);return current_n->value; }
struct sfixture {
    int created;uint32_t selected;void *table;void *entries[4];uint32_t ids[4];
    unsigned count;struct {unsigned id;uint32_t a;} trace[8];
};
static struct sfixture *current_s;
static void semit(unsigned id,uint32_t a)
{ assert(current_s->count<8);current_s->trace[current_s->count].id=id;current_s->trace[current_s->count].a=a;++current_s->count; }
enum {S_CREATED,S_SEL,S_TABLE,S_ENTRY,S_ID};
static int sq_created(void *list) { semit(S_CREATED,(uint32_t)(uintptr_t)list);return current_s->created; }
static uint32_t sq_sel(void *list) { semit(S_SEL,(uint32_t)(uintptr_t)list);return current_s->selected; }
static void *sq_table(void *list) { semit(S_TABLE,(uint32_t)(uintptr_t)list);return current_s->table; }
static void *sq_entry(void *table,uint32_t i)
{ semit(S_ENTRY,i);(void)table;assert(i<4);return current_s->entries[i]; }
static uint32_t sq_id(void *entry)
{
    for(unsigned i=0;i<4;++i)if(current_s->entries[i]==entry) { semit(S_ID,i);return current_s->ids[i]; }
    assert(0);return 0;
}
int main(void)
{
    int error=42;struct nfixture nf={0};
    /* EINVAL paths run no callbacks. */
    assert(!oa_setting_row_numeric(NULL,&error) && error==EINVAL);
    struct oa_svg_row_text_ops nbad={0};
    current_n=&nf;
    assert(!oa_setting_row_numeric(&nbad,&error) && error==EINVAL && !nf.count);
    assert(oa_setting_list_selected_id(NULL,&error)==UINT32_C(0xffffffff) && error==EINVAL);
    struct oa_svg_list_select_ops sbad={0};
    assert(oa_setting_list_selected_id(&sbad,&error)==UINT32_C(0xffffffff) && error==EINVAL);
    /* Numeric forwards the embedded entry handle unchanged. */
    static unsigned char entryobj[8];nf.entry=entryobj;nf.value=0x1234abcd;
    static unsigned char rowobj[8];nf.row=rowobj;
    struct oa_svg_row_text_ops nops={&nf,nq_entry,nq_get};
    assert(oa_setting_row_numeric(&nops,&error)==0x1234abcd && !error);
    assert(nf.count==2 && nf.trace[0].id==N_ENTRY);
    assert(nf.trace[1].id==N_GET && nf.trace[1].a==(uint32_t)(uintptr_t)entryobj);
    /* Not created short-circuits before any other query. */
    static unsigned char listobj[8];struct sfixture sf={0};
    for(unsigned i=0;i<4;++i)sf.entries[i]=(void *)(uintptr_t)(0x3000u+i);
    struct oa_svg_list_select_ops sops={&sf,sq_created,sq_sel,sq_table,sq_entry,sq_id};
    current_s=&sf;
    sf.created=0;sf.selected=2;
    assert(oa_setting_list_selected_id(&sops,&error)==UINT32_C(0xffffffff) && !error && sf.count==1);
    /* Selected -1 (wrapping) short-circuits before the table walk. */
    sf.created=1;sf.selected=UINT32_MAX;sf.count=0;
    assert(oa_setting_list_selected_id(&sops,&error)==UINT32_C(0xffffffff) && !error && sf.count==2);
    /* Normal path walks table[index] and reads its first word. */
    static unsigned char tableobj[16];sf.table=tableobj;
    sf.selected=2;sf.ids[2]=0x77;sf.count=0;
    assert(oa_setting_list_selected_id(&sops,&error)==0x77 && !error && sf.count==5);
    assert(sf.trace[2].id==S_TABLE && sf.trace[3].id==S_ENTRY && sf.trace[3].a==2);
    assert(sf.trace[4].id==S_ID && sf.trace[4].a==2);
    (void)listobj;
    puts("ok SVG list widgets: numeric forward, created/none gates and table-walk id");
    return 0;
}
