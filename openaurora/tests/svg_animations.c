/* Boundary and golden-field checks for the offline firmware-format reader. */
#include <assert.h>
#include <unistd.h>
#define main svg_animation_cli_main
#include "../tools/re/svg-animations.c"
#undef main

static void prefixes(const unsigned char *data, size_t size)
{
    for (size_t n = 1; n < size; n++) {
        struct cursor c = { data, n, 0 };
        assert(element(&c, 1, 100) == -1);
        assert(c.pos <= c.size);
    }
}
int main(void)
{
    const unsigned char animation[] = {
        10, 'a', 0, 1, 255, 255, '#', 't', 0, 'x', 0, 0, 254, 255,
        '0', '.', '3', 0, '0', 0, '0', 0, '4', '0', 0,
        'l', 'i', 'n', 'e', 'a', 'r', 0, 0
    };
    const unsigned char rectangle[] = {
        1, 'r', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 255, 255
    };
    prefixes(animation, sizeof animation);
    prefixes(rectangle, sizeof rectangle);
    unsigned char unknown = 12;
    struct cursor c = { &unknown, 1, 0 };
    assert(element(&c, 1, 0) == -1);
    unsigned char end = 9;
    c = (struct cursor){ &end, 1, 0 };
    assert(element(&c, 1, 0) == 0 && c.pos == 1);
    c = (struct cursor){ rectangle, sizeof rectangle, 0 };
    assert(element(&c, 1, 0) == 0 && c.pos == sizeof rectangle);

    FILE *capture = tmpfile();
    int saved = dup(STDOUT_FILENO);
    assert(capture && saved >= 0 && fflush(stdout) == 0);
    assert(dup2(fileno(capture), STDOUT_FILENO) == STDOUT_FILENO);
    c = (struct cursor){ animation, sizeof animation, 0 };
    assert(element(&c, 1, 100) == 0 && c.pos == sizeof animation);
    unsigned char signed_rect[sizeof rectangle];
    memcpy(signed_rect, rectangle, sizeof rectangle);
    signed_rect[3] = signed_rect[4] = signed_rect[13] = 255;
    rectangles = 1;
    c = (struct cursor){ signed_rect, sizeof signed_rect, 0 };
    assert(element(&c, 1, 100) == 0 && c.pos == sizeof signed_rect);
    rectangles = 0;
    assert(fflush(stdout) == 0);
    assert(dup2(saved, STDOUT_FILENO) == STDOUT_FILENO);
    close(saved);
    rewind(capture);
    char row[256];
    assert(fgets(row, sizeof row, capture));
    assert(!strcmp(row, "0x00000001\t100\t10\ta\t1\t-1\t#t\tx\t\t-2\t0.3\t0\t0\t40\tlinear\t\n"));
    assert(fgets(row, sizeof row, capture));
    assert(!strcmp(row, "0x00000001\t100\tr\t-1\t0\t0\t0\t0\t-1\t\t\t\t\n"));
    assert(fgetc(capture) == EOF);
    fclose(capture);
    puts("  ok   BinSVG animation golden fields/sign extension; all truncated prefixes reject safely");
    return 0;
}
