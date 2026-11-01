/*
 * bootsplash - native boot screen for the MB230, drawn straight onto OSD_NORMAL2 (/dev/fb1).
 *
 * Called by /conf/remotelf/aurora.elf immediately before `exec aurora.elf`, so the set shows an
 * OmarchyTV-branded screen during the ~40-60 s the stock stack needs before the web launcher exists.
 * aurora repaints the layer when it starts, so this must be fast and stateless - no cleanup.
 *
 * Build (static, no libc deps beyond musl):
 *   arm-linux-musleabihf-gcc -static -Os -s -o bootsplash splash.c
 *
 * fb1: 1920x2160 = 2 x 1080 frames, 32bpp, stride 7680, byte order R,G,B,A (same as tvshot --layer 1).
 * Both frames are painted so whichever is scanned out shows the splash.
 *
 *   bootsplash                 # draw the default screen
 *   bootsplash "text" "sub"    # override the two lines
 *   bootsplash --blank         # fill with the background only
 */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define W 1920
#define H 1080
#define FRAMES 2
#define STRIDE (W * 4)

static const uint32_t BG = 0x11151F;      /* RGB #11151f; px() stores BGRA bytes */
static const uint32_t FG = 0xE6EDF3;
static const uint32_t ACCENT = 0x7AA2F7;
static const uint32_t DIM = 0x565F89;

/* 5x7 bitmap font: A-Z, 0-9, space, . - : */
static const char *CHARSET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .-:";
static const unsigned char FONT[][7] = {
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* A */
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, /* B */
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, /* C */
    {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C}, /* D */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, /* E */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, /* F */
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}, /* G */
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* H */
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* I */
    {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}, /* J */
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, /* K */
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, /* L */
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, /* M */
    {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11}, /* N */
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* O */
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, /* P */
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, /* Q */
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, /* R */
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}, /* S */
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* T */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* U */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, /* V */
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}, /* W */
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, /* X */
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, /* Y */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, /* Z */
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, /* 0 */
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* 1 */
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, /* 2 */
    {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}, /* 3 */
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, /* 4 */
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, /* 5 */
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, /* 6 */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, /* 7 */
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, /* 8 */
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}, /* 9 */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* space */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}, /* . */
    {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}, /* - */
    {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}, /* : */
};

static uint8_t *fb;

static inline void px(int x, int y, uint32_t c)
{
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    for (int f = 0; f < FRAMES; f++) {
        uint8_t *p = fb + (size_t)f * STRIDE * H + (size_t)y * STRIDE + (size_t)x * 4;
        p[0] = c & 0xff;           /* B */
        p[1] = (c >> 8) & 0xff;    /* G */
        p[2] = (c >> 16) & 0xff;   /* R */
        p[3] = 0xff;               /* A */
    }
}

static void rect(int x, int y, int w, int h, uint32_t c)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) px(x + i, y + j, c);
}

static const unsigned char *glyph(char ch)
{
    if (ch >= 'a' && ch <= 'z') ch -= 32;
    for (int i = 0; CHARSET[i]; i++)
        if (CHARSET[i] == ch) return FONT[i];
    return FONT[36];               /* space */
}

static int text_width(const char *s, int scale)
{
    int n = (int)strlen(s);
    return n ? n * 6 * scale - scale : 0;
}

static void text(const char *s, int cx, int y, int scale, uint32_t c)
{
    int x = cx - text_width(s, scale) / 2;
    for (const char *p = s; *p; p++) {
        const unsigned char *g = glyph(*p);
        for (int row = 0; row < 7; row++)
            for (int col = 0; col < 5; col++)
                if (g[row] & (1 << (4 - col)))
                    rect(x + col * scale, y + row * scale, scale, scale, c);
        x += 6 * scale;
    }
}

int main(int argc, char **argv)
{
    const char *line1 = "OMARCHY TV";
    const char *line2 = "VESTEL MB230";
    int blank = 0;

    if (argc > 1 && strcmp(argv[1], "--blank") == 0) blank = 1;
    if (argc > 1 && !blank) line1 = argv[1];
    if (argc > 2) line2 = argv[2];

    int fd = open("/dev/fb1", O_RDWR);
    if (fd < 0) { perror("open /dev/fb1"); return 1; }

    size_t len = (size_t)STRIDE * H * FRAMES;
    fb = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) { perror("mmap"); close(fd); return 1; }

    rect(0, 0, W, H, BG);
    if (!blank) {
        text(line1, W / 2, 380, 22, FG);          /* wordmark */
        rect(W / 2 - 260, 620, 520, 4, ACCENT);   /* rule */
        text(line2, W / 2, 670, 6, ACCENT);       /* model */
        text("STARTING", W / 2, 760, 4, DIM);     /* state */
    }

    msync(fb, len, MS_SYNC);
    munmap(fb, len);
    close(fd);
    return 0;
}
