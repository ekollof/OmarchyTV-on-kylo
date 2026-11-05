/* Static PNG and baseline JPEG decoding into top-down straight RGBA.
 *
 * PNG conversion follows Aurora b30918: expand/pack low depths, strip16 (not
 * scale16), gray/palette -> RGB, tRNS -> alpha or opaque filler. No gamma
 * conversion. Uses the shipped libpng16 classic API, not Aurora's built-in
 * Cabot codec. JPEG uses the vendor libjpeg-turbo TurboJPEG API (2.0.1 on the
 * TV); CMYK/YCCK and animated PNG are rejected rather than mis-rendered.
 *
 * Opaque library pointers and fixed-width public arguments avoid depending on
 * host library struct layouts. Synchronous loop-thread-only service; input
 * must be a bounded regular file. No hardware or network access. */
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "openaurora/module.h"
#include "openaurora/services.h"

#define MAX_DIM 8192u
#define MAX_FILE (16u*1024u*1024u)
#define MAX_PIXELS (64u*1024u*1024u)
typedef void (*png_callback)(void *,const char *);
typedef void (*png_reader)(void *,unsigned char *,size_t);
struct ctx {
    const oa_host_api *api;
    void *lib;
    void *zlib;
    void *tjlib;
    int published,suspended;
    void *(*tj_init)(void);
    int (*tj_header)(void *,const unsigned char *,unsigned long,int *,int *,int *,int *);
    int (*tj_decompress)(void *,const unsigned char *,unsigned long,
        unsigned char *,int,int,int,int,int);
    int (*tj_destroy)(void *);
    const char *(*version)(const void *);
    void *(*create_read)(const char *,void *,png_callback,png_callback);
    void *(*create_info)(void *);
    void (*destroy_read)(void **,void **,void **);
    void *(*error_ptr)(const void *);
    void *(*io_ptr)(const void *);
    void (*set_read)(void *,void *,png_reader);
    void (*read_info)(void *,void *);
    uint32_t (*ihdr)(const void *,const void *,uint32_t *,uint32_t *,
        int *,int *,int *,int *,int *);
    uint32_t (*valid)(const void *,const void *,uint32_t);
    void (*expand_gray)(void *);
    void (*pack)(void *);
    void (*strip16)(void *);
    void (*gray_rgb)(void *);
    void (*palette_rgb)(void *);
    void (*trans_alpha)(void *);
    void (*filler)(void *,uint32_t,int);
    int (*interlace)(void *);
    void (*update_info)(void *,void *);
    size_t (*rowbytes)(const void *,const void *);
    unsigned char (*channels)(const void *,const void *);
    void (*read_image)(void *,unsigned char **);
    void (*read_end)(void *,void *);
};
struct run {
    jmp_buf jump;
    unsigned char *input,*pixels,**rows;
    size_t size,at;
    void *png,*info;
    int result;
};
static struct ctx *current;
static void png_error(void *png,const char *message)
{
    struct run *r=current->error_ptr(png);
    (void)message;
    longjmp(r->jump,1);
}
static void png_warning(void *png,const char *message)
{ (void)png;(void)message; }
static void png_read(void *png,unsigned char *out,size_t size)
{
    struct run *r=current->io_ptr(png);
    if(size>r->size-r->at)longjmp(r->jump,1);
    memcpy(out,r->input+r->at,size);r->at+=size;
}
static int read_regular(const char *path,unsigned char **out,size_t *len)
{
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NONBLOCK);
    if(fd<0)return -errno;
    struct stat st;
    if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_size<8 ||
        (uint64_t)st.st_size>MAX_FILE) { close(fd);return -EINVAL; }
    size_t size=(size_t)st.st_size;
    unsigned char *buf=malloc(size);
    if(!buf){close(fd);return -ENOMEM;}
    size_t at=0;
    while(at<size) {
        ssize_t got=read(fd,buf+at,size-at);
        if(got<0 && errno==EINTR)continue;
        if(got<=0){close(fd);free(buf);return -EIO;}
        at+=(size_t)got;
    }
    close(fd);
    *out=buf;
    *len=size;
    return 0;
}
static int read_file(const char *path,struct run *r)
{
    int rc=read_regular(path,&r->input,&r->size);
    if(rc)return rc;
    r->at=0;
    static const unsigned char signature[]={137,80,78,71,13,10,26,10};
    if(memcmp(r->input,signature,8))return -EINVAL;
    /* Do not silently reduce an animated image to a static first frame. */
    size_t at=8;
    while(r->size-at>=12) {
        const unsigned char *p=r->input+at;
        uint32_t n=(uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];
        if(n>r->size-at-12)return -EINVAL;
        if(!memcmp(p+4,"acTL",4))return -ENOTSUP;
        at+=(size_t)n+12;
    }
    return 0;
}
static void release_pixels(void *pixels) { free(pixels); }
static int decode_png(const char *path,int *w,int *h,unsigned char **out)
{
    struct ctx *c=current;
    if(w)*w=0;
    if(h)*h=0;
    if(out)*out=NULL;
    if(!c || !c->published || !path || !*path || !w || !h || !out)return -EINVAL;
    if(c->suspended)return -EAGAIN;
    struct run *r=calloc(1,sizeof *r);
    if(!r)return -ENOMEM;
    r->result=read_file(path,r);
    if(r->result)goto done;
    r->result=-EINVAL;
    /* Mutated resources live on the heap: automatic values modified after
     * setjmp would be indeterminate after longjmp. */
    if(setjmp(r->jump))goto done;
    r->png=c->create_read(c->version(NULL),r,png_error,png_warning);
    if(!r->png)goto done;
    r->info=c->create_info(r->png);
    if(!r->info)goto done;
    c->set_read(r->png,r,png_read);c->read_info(r->png,r->info);
    uint32_t width=0,height=0;int depth,color,interlace,compression,filter;
    if(!c->ihdr(r->png,r->info,&width,&height,&depth,&color,&interlace,&compression,&filter) ||
        !width || !height || width>MAX_DIM || height>MAX_DIM ||
        (uint64_t)width*height*4>MAX_PIXELS)goto done;
    if(depth<8) {
        if(color==0)c->expand_gray(r->png);
        else c->pack(r->png);
    } else if(depth!=8)c->strip16(r->png);
    if(!(color&2))c->gray_rgb(r->png);
    if(color&1)c->palette_rgb(r->png);
    if(!(color&4)) {
        if(c->valid(r->png,r->info,0x10))c->trans_alpha(r->png);
        else c->filler(r->png,255,1);
    }
    (void)c->interlace(r->png);c->update_info(r->png,r->info);
    size_t stride=(size_t)width*4;
    if(c->rowbytes(r->png,r->info)!=stride || c->channels(r->png,r->info)!=4)goto done;
    r->pixels=malloc(stride*height);r->rows=malloc((size_t)height*sizeof *r->rows);
    if(!r->pixels || !r->rows){r->result=-ENOMEM;goto done;}
    for(uint32_t y=0;y<height;++y)r->rows[y]=r->pixels+(size_t)y*stride;
    c->read_image(r->png,r->rows);c->read_end(r->png,NULL);
    *w=(int)width;*h=(int)height;*out=r->pixels;r->pixels=NULL;r->result=0;
done:
    if(r->png)c->destroy_read(&r->png,&r->info,NULL);
    int result=r->result;
    free(r->input);free(r->pixels);free(r->rows);free(r);
    return result;
}
#define TJ_PF_RGBA 7
#define TJ_FLAG_FASTDCT 2048
#define TJ_CS_CMYK 3
#define TJ_CS_YCCK 4
static int decode_jpeg(const char *path,int *w,int *h,unsigned char **out)
{
    struct ctx *c=current;
    if(w)*w=0;
    if(h)*h=0;
    if(out)*out=NULL;
    if(!c || !c->published || !path || !*path || !w || !h || !out)return -EINVAL;
    if(c->suspended)return -EAGAIN;
    if(!c->tj_init || !c->tj_header || !c->tj_decompress || !c->tj_destroy)
        return -ENOTSUP;
    unsigned char *input=NULL;
    size_t len=0;
    int rc=read_regular(path,&input,&len);
    if(rc)return rc;
    void *tj=c->tj_init();
    if(!tj){free(input);return -ENOMEM;}
    int width=0,height=0,subsamp=0,space=0;
    if(c->tj_header(tj,input,(unsigned long)len,&width,&height,&subsamp,&space) ||
        width<=0 || height<=0 || width>(int)MAX_DIM || height>(int)MAX_DIM ||
        (uint64_t)width*height*4>MAX_PIXELS ||
        space==TJ_CS_CMYK || space==TJ_CS_YCCK) {
        c->tj_destroy(tj);free(input);return -ENOTSUP;
    }
    size_t bytes=(size_t)width*(size_t)height*4u;
    unsigned char *pixels=malloc(bytes);
    if(!pixels){c->tj_destroy(tj);free(input);return -ENOMEM;}
    if(c->tj_decompress(tj,input,(unsigned long)len,pixels,width,width*4,
            height,TJ_PF_RGBA,TJ_FLAG_FASTDCT)) {
        c->tj_destroy(tj);free(input);free(pixels);return -EINVAL;
    }
    c->tj_destroy(tj);free(input);
    *w=width;*h=height;*out=pixels;
    return 0;
}
static const oa_image_decode_iface iface={decode_png,release_pixels,decode_jpeg};
static int symbol(void *lib,const char *name,void *out,size_t size)
{
    void *p=dlsym(lib,name);
    if(!p || size!=sizeof p)return -1;
    memcpy(out,&p,size);return 0;
}
static void *open_png_lib(const char *configured,void **zlib_out)
{
    void *lib=dlopen(configured?configured:"libpng16.so.16",RTLD_NOW|RTLD_LOCAL);
    if(lib || configured)return lib;
    void *zlib=dlopen("/vendor/lib/libs/libz.so",RTLD_NOW|RTLD_GLOBAL);
    lib=dlopen("/vendor/lib/libs/libpng16.so.16",RTLD_NOW|RTLD_LOCAL);
    if(!lib){if(zlib)dlclose(zlib);return NULL;}
    *zlib_out=zlib;
    return lib;
}
static int m_init(const oa_host_api *api,void **out)
{
    if(!api || api->abi!=OA_ABI_VERSION || api->size<sizeof *api)return -1;
    struct ctx *c=calloc(1,sizeof *c);if(!c)return -1;
    c->api=api;*out=c;current=c;return 0;
}
static void m_stop(void *v)
{
    struct ctx *c=v;
    if(c->published)c->api->unpublish(c->api->priv,OA_SVC_IMAGE_DECODE);
    c->published=0;
    if(c->tjlib)dlclose(c->tjlib);
    c->tjlib=NULL;c->tj_init=NULL;c->tj_header=NULL;
    c->tj_decompress=NULL;c->tj_destroy=NULL;
    if(c->lib)dlclose(c->lib);
    c->lib=NULL;
    if(c->zlib)dlclose(c->zlib);
    c->zlib=NULL;
}
static void *open_turbojpeg(void)
{
    static const char *paths[]={
        "libturbojpeg.so.0",
        "/vendor/com.nvt.nflx/lib/libturbojpeg.so.0",
        "/vendor/lib/libs/libturbojpeg.so.0",
    };
    for(size_t i=0;i<sizeof paths/sizeof paths[0];++i) {
        void *lib=dlopen(paths[i],RTLD_NOW|RTLD_LOCAL);
        if(lib)return lib;
    }
    return NULL;
}
static int m_start(void *v)
{
    struct ctx *c=v;
    const char *name=c->api->config_get(c->api->priv,"image.lib",NULL);
    c->lib=open_png_lib(name,&c->zlib);
    if(!c->lib)return -1;
#define LOAD(field,name) if(symbol(c->lib,name,&c->field,sizeof c->field))goto failed
    LOAD(version,"png_get_libpng_ver");
    LOAD(create_read,"png_create_read_struct");LOAD(create_info,"png_create_info_struct");
    LOAD(destroy_read,"png_destroy_read_struct");
    LOAD(error_ptr,"png_get_error_ptr");LOAD(io_ptr,"png_get_io_ptr");
    LOAD(set_read,"png_set_read_fn");LOAD(read_info,"png_read_info");
    LOAD(ihdr,"png_get_IHDR");LOAD(valid,"png_get_valid");
    LOAD(expand_gray,"png_set_expand_gray_1_2_4_to_8");LOAD(pack,"png_set_packing");
    LOAD(strip16,"png_set_strip_16");LOAD(gray_rgb,"png_set_gray_to_rgb");
    LOAD(palette_rgb,"png_set_palette_to_rgb");LOAD(trans_alpha,"png_set_tRNS_to_alpha");
    LOAD(filler,"png_set_filler");LOAD(interlace,"png_set_interlace_handling");
    LOAD(update_info,"png_read_update_info");LOAD(rowbytes,"png_get_rowbytes");
    LOAD(channels,"png_get_channels");LOAD(read_image,"png_read_image");LOAD(read_end,"png_read_end");
#undef LOAD
    if(strncmp(c->version(NULL),"1.6.",4))goto failed;
    c->tjlib=open_turbojpeg();
    if(c->tjlib &&
        (symbol(c->tjlib,"tjInitDecompress",&c->tj_init,sizeof c->tj_init) ||
         symbol(c->tjlib,"tjDecompressHeader3",&c->tj_header,sizeof c->tj_header) ||
         symbol(c->tjlib,"tjDecompress2",&c->tj_decompress,sizeof c->tj_decompress) ||
         symbol(c->tjlib,"tjDestroy",&c->tj_destroy,sizeof c->tj_destroy))) {
        dlclose(c->tjlib);
        c->tjlib=NULL;c->tj_init=NULL;c->tj_header=NULL;
        c->tj_decompress=NULL;c->tj_destroy=NULL;
    }
    if(!c->tjlib)
        c->api->log(c->api->priv,OA_LOG_WARN,
            "image_decode: TurboJPEG unavailable; PNG only");
    if(c->api->publish(c->api->priv,OA_SVC_IMAGE_DECODE,2,&iface))goto failed;
    c->published=1;c->suspended=0;return 0;
failed:
    m_stop(c);return -1;
}
static void m_destroy(void *v)
{ m_stop(v);if(current==v)current=NULL;free(v); }
static int m_suspend(void *v) { ((struct ctx *)v)->suspended=1;return 0; }
static int m_resume(void *v) { ((struct ctx *)v)->suspended=0;return 0; }
static const oa_module module={
    .magic=OA_MODULE_MAGIC,.abi=OA_ABI_VERSION,.size=sizeof(oa_module),
    .name="image_decode",.version="2",.init=m_init,.start=m_start,.stop=m_stop,
    .destroy=m_destroy,.suspend=m_suspend,.resume=m_resume
};
OA_MODULE_EXPORT(module)
