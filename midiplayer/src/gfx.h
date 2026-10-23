// Drawing the player's 640x480 8bpp frame: the core's font (Cozette 6x13,
// hybrid/rtl/hybrid_wall_font.hex) and the colours of docs/midiplayer.md
#ifndef MP_GFX_H
#define MP_GFX_H

#include <stdint.h>

#define GFX_W 640
#define GFX_H 480
#define GFX_CW 6       // a character's cell
#define GFX_CH 13
#define GFX_PITCH 16   // rows of text

enum {
    C_BG,              // the core's background; transparent on the overlay (host version 17)
    C_INK,
    C_DIM,
    C_FAINT,
    C_SEL,
    C_HI,
    C_BLACK,
    C_WHITE,
};

void gfx_palette(uint32_t palette[256]);
void gfx_clear(uint8_t* fb);
void gfx_fill(uint8_t* fb, int x, int y, int w, int h, int colour);
void gfx_frame(uint8_t* fb, int x, int y, int w, int h, int colour);
// A box with rounded corners as the core's badge has: an edge of one pixel
// in `edge`, filled with `inside`, the corners smoothed against what is
// under them
void gfx_round(uint8_t* fb, int x, int y, int w, int h, int edge, int inside);
// Text from (x, y), cut with "..." to `maxw` pixels (0: no limit); returns
// where it ends
int gfx_text(uint8_t* fb, int x, int y, const char* s, int colour, int maxw);
// Text that ends at x
int gfx_rtext(uint8_t* fb, int x, int y, const char* s, int colour);
// A bar `frac` full
void gfx_bar(uint8_t* fb, int x, int y, int w, int h, float frac, int colour, int back);
void gfx_cursor(uint8_t* fb, int x, int y);

#endif
