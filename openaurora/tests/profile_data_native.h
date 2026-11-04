/* Offline byte-payload reader differential; no vendor bytes embedded. */
static void pd_word(unsigned char *p,uint32_t value)
{ for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(value>>(8*i)); }
static void check_profile_data(const unsigned char *elf,size_t bytes)
{
    const struct {uint32_t address,length,crc,guard_literal;} fragments[]={
        {0xc5e100,144,UINT32_C(0x39caf1e3),0xc5e188},
        {0xc5e298,108,UINT32_C(0x56b95047),0xc5e300},
        {0xc5e350,100,UINT32_C(0x1a1b3c40),0xc5e3b0}
    };
    unsigned char *code=page(0xc5e000,4096),*divide=page(0x1285000,4096);
    unsigned char *libc_page=page(0x6b0000,4096),*copy_page=page(0x6b3000,4096);
    uint32_t gp=(uint32_t)(uintptr_t)&guard;
    for(unsigned i=0;i<3;++i) {
        size_t off=file_offset(elf,bytes,fragments[i].address,fragments[i].length);
        assert(crc32(elf+off,fragments[i].length)==fragments[i].crc);
        memcpy(stub_destination(code,0xc5e000,fragments[i].address,fragments[i].length),elf+off,fragments[i].length);
        memcpy(stub_destination(code,0xc5e000,fragments[i].guard_literal,4),&gp,4);
    }
    size_t off=file_offset(elf,bytes,0x128547e,682);
    assert(crc32(elf+off,682)==UINT32_C(0x4c973710));memcpy(divide+0x47e,elf+off,682);
    const uint32_t strlen_stub[]={UINT32_C(0xe59fc000),UINT32_C(0xe12fff1c),bits(reference_strlen)};
    memcpy(stub_destination(libc_page,0x6b0000,0x6b0a9c,sizeof strlen_stub),strlen_stub,sizeof strlen_stub);
    void *(*copy)(void *,const void *,size_t)=memcpy;uint32_t copy_address;memcpy(&copy_address,&copy,4);
    const uint32_t copy_stub[]={UINT32_C(0xe59fc000),UINT32_C(0xe12fff1c),copy_address};
    memcpy(stub_destination(copy_page,0x6b3000,0x6b3a28,sizeof copy_stub),copy_stub,sizeof copy_stub);
    executable(code,4096);executable(divide,4096);executable(libc_page,4096);executable(copy_page,4096);
    typedef unsigned (*read_data_fn)(void *,void *,const char *,void *,uint32_t);
    read_data_fn reference;uint32_t entry=0xc5e351;memcpy(&reference,&entry,4);
    unsigned char profile[512]={0};memcpy(profile,"EFSL",4);pd_word(profile+0x38,1);
    pd_word(profile+0x44,oa_profile_hash("Payload"));pd_word(profile+0x50,0x180);
    for(unsigned i=0;i<16;++i)profile[0x180+i]=(unsigned char)(i*17);
    uint32_t object[4]={0,0,0,(uint32_t)(uintptr_t)profile};
    const unsigned kinds[]={0,1,2,3,4,5,6,255};unsigned compared=0;
    for(unsigned kind=0;kind<8;++kind)for(unsigned length=0;length<=16;++length)
    for(unsigned capacity=0;capacity<=16;++capacity) {
        profile[0x48]=(unsigned char)kinds[kind];
        pd_word(profile+0x4c,kinds[kind]==1?0:length);
        unsigned char ours[24],vendor[24];memset(ours,0xa5,sizeof ours);memset(vendor,0xa5,sizeof vendor);
        const void *payload;size_t size;int present;
        assert(!oa_profile_data(profile,sizeof profile,"","Payload",&payload,&size,&present));
        unsigned success=reference(object,profile+0x30,"Payload",vendor+1,capacity);
        unsigned expected=present && size<=capacity;
        assert(success==expected);
        if(expected)memcpy(ours+1,payload,size);
        assert(!memcmp(ours,vendor,sizeof ours));++compared;
    }
    assert(!munmap(code,4096) && !munmap(divide,4096) && !munmap(libc_page,4096) && !munmap(copy_page,4096));
    printf("ok offline shipped ARM profile data: %u type/length/capacity cases, exact payload bytes and untouched guards\n",compared);
}
