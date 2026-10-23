// The frame's drawing
#include "gfx.h"

#include <string.h>

// characters 32..126, 13 rows each, bit 0 the leftmost pixel (made by the
// build from hybrid/rtl/hybrid_wall_font.hex)
static const uint8_t font[95 * GFX_CH] = {
#include "font.inc"
};

static const char* const arrow[] = {
    "X..........",
    "XX.........",
    "XOX........",
    "XOOX.......",
    "XOOOX......",
    "XOOOOX.....",
    "XOOOOOX....",
    "XOOOOOOX...",
    "XOOOOOOOX..",
    "XOOOOOOOOX.",
    "XOOOOOXXXXX",
    "XOOXOOX....",
    "XOX.XOOX...",
    "XX..XOOX...",
    "X....XOOX..",
    ".....XOOX..",
    "......XX...",
};

// Halfway between two of the colours above: index MIX + a * 8 + b
#define MIX 64
#define RADIUS 5

static int mix(int a, int b) {
    return a == b || a >= 8 || b >= 8 ? b : MIX + a * 8 + b;
}

void gfx_palette(uint32_t palette[256]) {
    int a, b, k;

    memset(palette, 0, 256 * sizeof *palette);
    palette[C_BG] = 0x7F30A0;
    palette[C_INK] = 0xEFEFEF;
    palette[C_DIM] = 0xC4A4DC;
    palette[C_FAINT] = 0x9C6CC0;
    palette[C_SEL] = 0x5A1E7A;
    palette[C_HI] = 0xFFD86A;
    palette[C_BLACK] = 0x000000;
    palette[C_WHITE] = 0xFFFFFF;
    for (a = 0; a < 8; a++) {
        for (b = 0; b < 8; b++) {
            uint32_t c = 0;
            for (k = 0; k < 24; k += 8) {
                c |= (((palette[a] >> k) & 255) + ((palette[b] >> k) & 255) + 1) / 2 << k;
            }
            palette[MIX + a * 8 + b] = c;
        }
    }
}

void gfx_clear(uint8_t* fb) {
    memset(fb, C_BG, GFX_W * GFX_H);
}

void gfx_fill(uint8_t* fb, int x, int y, int w, int h, int colour) {
    int r;

    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > GFX_W) {
        w = GFX_W - x;
    }
    if (y + h > GFX_H) {
        h = GFX_H - y;
    }
    for (r = 0; r < h && w > 0; r++) {
        memset(fb + (y + r) * GFX_W + x, colour, (size_t)w);
    }
}

void gfx_frame(uint8_t* fb, int x, int y, int w, int h, int colour) {
    gfx_fill(fb, x, y, w, 1, colour);
    gfx_fill(fb, x, y + h - 1, w, 1, colour);
    gfx_fill(fb, x, y, 1, h, colour);
    gfx_fill(fb, x + w - 1, y, 1, h, colour);
}

// A box with corners of radius r: each corner pixel the colour where the
// circle covers most of it, halfway where it covers about half
static void round_fill(uint8_t* fb, int x, int y, int w, int h, int r, int colour) {
    int row, col;

    for (row = 0; row < h; row++) {
        int py = y + row;
        if (py < 0 || py >= GFX_H) {
            continue;
        }
        for (col = 0; col < w; col++) {
            int px = x + col, cx = col < r ? r - col : col >= w - r ? col - (w - r - 1) : 0;
            int cy = row < r ? r - row : row >= h - r ? row - (h - r - 1) : 0, inside = 0, sx, sy;
            uint8_t* p;

            if (px < 0 || px >= GFX_W) {
                continue;
            }
            p = &fb[py * GFX_W + px];
            if (cx == 0 || cy == 0) {
                *p = (uint8_t)colour;
                continue;
            }
            // the share of the pixel inside the circle, from 4x4 points
            for (sy = 0; sy < 4; sy++) {
                for (sx = 0; sx < 4; sx++) {
                    float dx = cx - (sx + 0.5f) / 4, dy = cy - (sy + 0.5f) / 4;
                    inside += dx * dx + dy * dy <= (float)(r * r);
                }
            }
            if (inside >= 12) {
                *p = (uint8_t)colour;
            } else if (inside >= 4) {
                *p = (uint8_t)mix(*p, colour);
            }
        }
    }
}

void gfx_round(uint8_t* fb, int x, int y, int w, int h, int edge, int inside) {
    round_fill(fb, x, y, w, h, RADIUS, edge);
    if (inside != edge) {
        round_fill(fb, x + 1, y + 1, w - 2, h - 2, RADIUS - 1, inside);
    }
}

static void glyph(uint8_t* fb, int x, int y, int c, int colour) {
    const uint8_t* g;
    int r, b;

    if (c < 32 || c > 126) {
        c = '?';
    }
    g = font + (c - 32) * GFX_CH;
    if (x < 0 || y < 0 || x + GFX_CW > GFX_W || y + GFX_CH > GFX_H) {
        return;
    }
    for (r = 0; r < GFX_CH; r++) {
        uint8_t* row = fb + (y + r) * GFX_W + x;
        for (b = 0; b < GFX_CW; b++) {
            if ((g[r] >> b) & 1) {
                row[b] = (uint8_t)colour;
            }
        }
    }
}

int gfx_text(uint8_t* fb, int x, int y, const char* s, int colour, int maxw) {
    int n = (int)strlen(s), i, cut = 0;

    if (maxw > 0 && n * GFX_CW > maxw) {
        n = maxw / GFX_CW - 3;
        cut = 1;
        if (n < 0) {
            n = 0;
        }
    }
    for (i = 0; i < n; i++) {
        glyph(fb, x + i * GFX_CW, y, (unsigned char)s[i], colour);
    }
    for (i = 0; cut && i < 3; i++) {
        glyph(fb, x + (n + i) * GFX_CW, y, '.', colour);
    }
    return x + (n + 3 * cut) * GFX_CW;
}

int gfx_rtext(uint8_t* fb, int x, int y, const char* s, int colour) {
    return gfx_text(fb, x - (int)strlen(s) * GFX_CW, y, s, colour, 0);
}

void gfx_bar(uint8_t* fb, int x, int y, int w, int h, float frac, int colour, int back) {
    int full = (int)(w * (frac < 0 ? 0 : frac > 1 ? 1 : frac) + 0.5f);

    gfx_fill(fb, x, y, w, h, back);
    gfx_fill(fb, x, y, full, h, colour);
}

void gfx_cursor(uint8_t* fb, int x, int y) {
    int r, c;

    for (r = 0; r < (int)(sizeof arrow / sizeof arrow[0]); r++) {
        for (c = 0; arrow[r][c] != 0; c++) {
            int px = x + c, py = y + r;
            if (px >= 0 && py >= 0 && px < GFX_W && py < GFX_H && arrow[r][c] != '.') {
                fb[py * GFX_W + px] = arrow[r][c] == 'X' ? C_BLACK : C_WHITE;
            }
        }
    }
}
