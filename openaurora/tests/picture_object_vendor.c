/* Execute the shipped Aurora ELF's UserPreferences/NovoPictureSettings picture
 * accessors under qemu-arm-static and compare them with common/picture_config.c
 * on the same serialized settingsDB blob.
 *
 * The vendor binary is ET_EXEC at 0x10000. We map its LOAD segments at their
 * link addresses, patch the memcpy@plt GOT slot, disable the cached HDR
 * profile flag, and call the Thumb functions with bit 0 set.
 *
 * The synthetic UserPreferences object holds the 20 x 0x240 config array at
 * +0x43f0, exactly as getPictureSettingConfiguration returns it. A fake
 * NovoPictureSettings vtable supplies only isFullModeApplicable (slot 0x4c)
 * for getZoomMode(format 0); all field logic is the shipped code.
 */
#define _GNU_SOURCE
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <ucontext.h>
#include <unistd.h>

#include "openaurora/picture_config.h"

#define OBJ_BYTES 0x8000u
#define CONFIG_BASE 0x43f0u
#define CONFIG_STRIDE OA_PICTURE_STRIDE

static unsigned char *g_vendor;
static unsigned char *g_model;
static const unsigned char *g_blob;
static int g_last_zoom;
static int g_full;
static int g_failures;

static void die(const char *what)
{
    fprintf(stderr, "picture object: %s: %s\n", what, strerror(errno));
    exit(2);
}

static void segv(int sig, siginfo_t *si, void *uc)
{
    (void)sig;
    ucontext_t *c = uc;
    fprintf(stderr, "picture object: SIGSEGV addr=%p pc=%#lx sp=%#lx lr=%#lx\n",
            si->si_addr, (unsigned long)c->uc_mcontext.arm_pc,
            (unsigned long)c->uc_mcontext.arm_sp,
            (unsigned long)c->uc_mcontext.arm_lr);
    _exit(3);
}

/* Fake NovoPictureSettings::isFullModeApplicable(index). */
static int full_stub(void *this, int index)
{
    (void)this;
    (void)index;
    return g_full;
}

static void *map_vendor(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) die("open aurora.elf");
    Elf32_Ehdr eh;
    if (pread(fd, &eh, sizeof eh, 0) != (ssize_t)sizeof eh ||
        memcmp(eh.e_ident, ELFMAG, SELFMAG) ||
        eh.e_machine != EM_ARM || eh.e_type != ET_EXEC) {
        fprintf(stderr, "picture object: not an ARM ET_EXEC\n");
        exit(2);
    }
    Elf32_Phdr ph[16];
    if (eh.e_phnum > 16 ||
        pread(fd, ph, (size_t)eh.e_phnum * sizeof ph[0], eh.e_phoff) !=
            (ssize_t)(eh.e_phnum * sizeof ph[0]))
        die("read phdrs");
    for (unsigned i = 0; i < eh.e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD) continue;
        uintptr_t start = (uintptr_t)ph[i].p_vaddr & ~(uintptr_t)0xfff;
        size_t delta = (size_t)((uintptr_t)ph[i].p_vaddr - start);
        size_t filesz = (size_t)ph[i].p_filesz + delta;
        size_t memsz = (size_t)ph[i].p_memsz + delta;
        off_t off = (off_t)ph[i].p_offset - (off_t)delta;
        if (memsz > filesz) {
            void *bss = mmap((void *)start, memsz, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
            if (bss == MAP_FAILED) die("mmap bss");
        }
        if (ph[i].p_flags & PF_X) {
            void *p = mmap((void *)start, filesz, PROT_READ | PROT_EXEC,
                           MAP_PRIVATE | MAP_FIXED, fd, off);
            if (p == MAP_FAILED) die("mmap file segment");
        } else {
            void *rw = mmap((void *)start, filesz, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_FIXED, fd, off);
            if (rw == MAP_FAILED) die("mmap rw segment");
        }
    }
    close(fd);

    /* memcpy@plt GOT slot and Profile_IsHDRPictureModesEnabled's cached
     * global (0 = disabled, so the non-HDR energy branch is executed). */
    void *(*memcpy_fn)(void *, const void *, size_t) = memcpy;
    memcpy((void *)(uintptr_t)0x17558c8, &memcpy_fn, sizeof memcpy_fn);
    *(int *)(uintptr_t)0x178c950 = 0;
    return (void *)1;
}

static int read_blob(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) die("open blob");
    struct stat st;
    if (fstat(fd, &st) || st.st_size != OA_PICTURE_BYTES) {
        fprintf(stderr, "picture object: blob is %lld bytes, expected %d\n",
                (long long)st.st_size, OA_PICTURE_BYTES);
        exit(2);
    }
    unsigned char *buf = malloc(OA_PICTURE_BYTES);
    if (!buf) die("malloc blob");
    size_t got = 0;
    while (got < OA_PICTURE_BYTES) {
        ssize_t n = read(fd, buf + got, OA_PICTURE_BYTES - got);
        if (n <= 0) die("read blob");
        got += (size_t)n;
    }
    close(fd);
    g_blob = buf;
    return 0;
}

#define CHECK(cond, ...)                                                     \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);             \
            fprintf(stderr, __VA_ARGS__);                                    \
            fputc('\n', stderr);                                             \
            g_failures++;                                                    \
        }                                                                    \
    } while (0)

static void *run_checks(void *arg)
{
    (void)arg;
    typedef const void *(*get_config_fn)(void *, int);
    typedef void (*set_config_fn)(void *, const void *, int);
    typedef int (*get_mode_fn)(void *, int);
    typedef void (*set_mode_fn)(void *, int, int);
    typedef int (*get_default_zoom_fn)(void *);
    typedef void (*set_default_zoom_fn)(void *, int);
    typedef int (*get_zoom_fn)(void *, int, int);
    typedef int (*get_energy_fn)(void *, int, int, int);
    typedef void (*set_energy_fn)(void *, int, int, int, int);

    get_config_fn get_config = (get_config_fn)0x00f7e1b1u;
    set_config_fn set_config = (set_config_fn)0x00f7e1edu;
    get_mode_fn get_mode = (get_mode_fn)0x00f7e45du;
    set_mode_fn set_mode = (set_mode_fn)0x00f7e499u;
    get_default_zoom_fn get_default_zoom = (get_default_zoom_fn)0x00f7bf51u;
    set_default_zoom_fn set_default_zoom = (set_default_zoom_fn)0x00f7bf85u;
    get_zoom_fn get_zoom = (get_zoom_fn)0x00fb64b5u;
    get_energy_fn get_energy = (get_energy_fn)0x00f7e8e5u;
    set_energy_fn set_energy = (set_energy_fn)0x00f7e9e5u;

    /* Fake NovoPictureSettings: vptr + the two fields getZoomMode reads. */
    uintptr_t *vtable = calloc(0x100, 1);
    if (!vtable) die("calloc vtable");
    union { int (*fn)(void *, int); uintptr_t u; } thumb_stub = {
        .fn = full_stub
    };
    vtable[0x4c / 4] = thumb_stub.u | 1u;
    *(uintptr_t *)g_vendor = (uintptr_t)vtable;
    *(uintptr_t *)(g_vendor + 0x78) = (uintptr_t)g_vendor;

    unsigned sources = 0, zoom_cases = 0, energy_cases = 0, setters = 0;
    for (int s = 0; s < OA_PICTURE_SOURCES; s++) {
        const unsigned char *cfg = get_config(g_vendor, s);
        CHECK(cfg == g_vendor + CONFIG_BASE + (size_t)s * CONFIG_STRIDE,
              "getPictureSettingConfiguration(%d) = %p", s, (const void *)cfg);
        CHECK(!memcmp(cfg, g_blob + (size_t)s * CONFIG_STRIDE, CONFIG_STRIDE),
              "config %d differs from the blob", s);
        sources++;

        int vm = get_mode(g_vendor, s);
        int mm = -1;
        CHECK(oa_picture_get_mode(g_blob, s, &mm) == 0, "model mode %d", s);
        CHECK(vm == mm, "source %d: vendor mode %d, model %d", s, vm, mm);
        CHECK(vm == *(const int *)(g_blob + (size_t)s * CONFIG_STRIDE +
                                   OA_PICTURE_OFF_MODE),
              "source %d: vendor mode %d not the raw field", s, vm);

        /* format 1 = +0x08, format 2 = +0x0c, format 0 = isFullModeApplicable. */
        int mv = -1;
        CHECK(oa_picture_get_zoom(g_blob, s, 0, &mv) == 0, "model zoom %d", s);
        CHECK(get_zoom(g_vendor, s, 1) == mv,
              "source %d: format 1 != +0x08", s);
        zoom_cases++;
        int mf = -1;
        CHECK(oa_picture_get_zoom(g_blob, s, 1, &mf) == 0, "model full %d", s);
        CHECK(get_zoom(g_vendor, s, 2) == mf,
              "source %d: format 2 != +0x0c", s);
        zoom_cases++;
        g_full = 0;
        CHECK(get_zoom(g_vendor, s, 0) == mv,
              "source %d: format 0/full=0 != +0x08", s);
        g_full = 1;
        CHECK(get_zoom(g_vendor, s, 0) == mf,
              "source %d: format 0/full=1 != +0x0c", s);
        zoom_cases += 2;

        for (int mode = 0; mode < 5; mode++) {
            for (int d3 = 0; d3 < 2; d3++) {
                int ve = get_energy(g_vendor, s, mode, d3);
                int me = -1;
                CHECK(oa_picture_get_energy(g_blob, s, mode, d3, &me) == 0,
                      "model energy %d/%d/%d", s, mode, d3);
                CHECK(ve == me, "source %d mode %d 3d %d: vendor %d model %d",
                      s, mode, d3, ve, me);
                energy_cases++;
            }
        }
    }

    CHECK(get_default_zoom(g_vendor) == g_last_zoom,
          "getDefaultZoomMode %d != %d", get_default_zoom(g_vendor),
          g_last_zoom);
    set_default_zoom(g_vendor, 9);
    CHECK(get_default_zoom(g_vendor) == 9 && *(int *)(g_vendor + 0x108) == 9,
          "setDefaultZoomMode round-trip");
    setters++;

    unsigned char *model_blob = g_model + CONFIG_BASE;
    for (int s = 0; s < OA_PICTURE_SOURCES; s++) {
        int raw = *(const int *)(g_blob + (size_t)s * CONFIG_STRIDE +
                                 OA_PICTURE_OFF_MODE);
        set_mode(g_vendor, 2, s);
        CHECK(oa_picture_set_mode(model_blob, s, 2) == 0, "model set mode");
        CHECK(!memcmp(g_vendor + CONFIG_BASE, model_blob, OA_PICTURE_BYTES),
              "setPictureMode source %d differs from the model", s);
        set_mode(g_vendor, raw, s);
        CHECK(oa_picture_set_mode(model_blob, s, raw) == 0, "model reset mode");
        CHECK(!memcmp(g_vendor + CONFIG_BASE, model_blob, OA_PICTURE_BYTES),
              "setPictureMode reset source %d differs", s);
        setters += 2;
    }

    for (int s = 0; s < OA_PICTURE_SOURCES; s++) {
        for (int mode = 0; mode < 5; mode++) {
            int off = (int)(CONFIG_BASE + (size_t)s * CONFIG_STRIDE) +
                      OA_PICTURE_OFF_ENERGY + mode * 0x70;
            int raw;
            memcpy(&raw, g_vendor + off, sizeof raw);
            set_energy(g_vendor, s, mode, 0, 4);
            CHECK(oa_picture_set_energy(model_blob, s, mode, 0, 4) == 0,
                  "model set energy");
            CHECK(!memcmp(g_vendor + CONFIG_BASE, model_blob,
                          OA_PICTURE_BYTES),
                  "setEnergySavingMode source %d mode %d differs from model",
                  s, mode);
            set_energy(g_vendor, s, mode, 0, raw);
            CHECK(oa_picture_set_energy(model_blob, s, mode, 0, raw) == 0,
                  "model reset energy");
            CHECK(!memcmp(g_vendor + CONFIG_BASE, model_blob,
                          OA_PICTURE_BYTES),
                  "setEnergySavingMode reset source %d mode %d differs", s,
                  mode);
            setters += 2;
        }
    }

    unsigned char temp[CONFIG_STRIDE];
    memcpy(temp, g_blob, sizeof temp);
    *(int *)(temp + OA_PICTURE_OFF_MODE) = 3;
    set_config(g_vendor, temp, 0);
    CHECK(oa_picture_set_config(model_blob, 0, temp) == 0,
          "model set config");
    CHECK(!memcmp(g_vendor + CONFIG_BASE, model_blob, OA_PICTURE_BYTES),
          "setPictureSettingConfiguration differs from model");
    CHECK(!memcmp(g_vendor + CONFIG_BASE + CONFIG_STRIDE,
                  g_blob + CONFIG_STRIDE, CONFIG_STRIDE),
          "setPictureSettingConfiguration touched source 1");
    setters++;

    printf("picture object: %u source configs, %u zoom formats, %u energy "
           "cases, %u setters\n", sources, zoom_cases, energy_cases, setters);
    return (void *)(uintptr_t)g_failures;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: %s aurora.elf blob.bin last_zoom\n", argv[0]);
        return 2;
    }
    struct sigaction sa = { .sa_sigaction = segv, .sa_flags = SA_SIGINFO };
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    read_blob(argv[2]);
    g_last_zoom = atoi(argv[3]);
    map_vendor(argv[1]);

    g_vendor = calloc(1, OBJ_BYTES);
    g_model = calloc(1, OBJ_BYTES);
    if (!g_vendor || !g_model) die("calloc object");
    memcpy(g_vendor + CONFIG_BASE, g_blob, OA_PICTURE_BYTES);
    memcpy(g_model + CONFIG_BASE, g_blob, OA_PICTURE_BYTES);
    *(int *)(g_vendor + 0x108) = g_last_zoom;
    *(int *)(g_model + 0x108) = g_last_zoom;
    static int fake_profile;
    *(uintptr_t *)(g_vendor + 0x7c38) = (uintptr_t)&fake_profile;

    pthread_t th;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 2u << 20);
    if (pthread_create(&th, &attr, run_checks, NULL)) die("pthread_create");
    void *result = NULL;
    pthread_join(th, &result);
    int failures = (int)(uintptr_t)result;
    if (failures) {
        fprintf(stderr, "picture object: %d failure(s)\n", failures);
        return 1;
    }
    puts("ok picture object: vendor UserPreferences/NovoPictureSettings accessors "
         "match the model");
    return 0;
}
