/* GENERATO da tools/make_assets.py - non modificare a mano */
#ifndef ASSETS_H
#define ASSETS_H

typedef struct { short w, h; const unsigned char *packed; } PackedBitmap;
typedef struct { short w, h, rowBytes; const unsigned char *bits; } Bitmap;
typedef struct { short w, h, rowBytes; const unsigned char *bits, *mask; } Sprite;

enum { BG_TITLE, BG_MAP, BG_DOCK, BG_TAVERN, BG_BEACH, BG_NIGHT, BG_COUNT };
#define ITEM_ICON_COUNT 8
#define ICON_SMALL 32
#define ICON_BIG 44
#define HERO_SIZES 3
#define HERO_FRAMES 9          /* fermo, parla, 6 camminata, occhi chiusi */
#define LANTERN_X 168
#define LANTERN_Y 96
#define LANTERN_FRAMES 4
#define STAR_COUNT 32           /* coordinate: x/2, y */
#define GULL_W 16
#define GULL_H 5
#define FONT_W 7
#define FONT_H 14
#define FONT_FIRST 32
#define GLYPH_ELLIPSIS 0x85
#define GLYPH_TRIANGLE 0x86

extern const PackedBitmap asset_bg[BG_COUNT];
extern const Bitmap asset_icon_small[ITEM_ICON_COUNT];
extern const Bitmap asset_icon_big[ITEM_ICON_COUNT];
extern const unsigned char *const asset_icon_mask; /* ICON_SMALL x ICON_SMALL ciascuna */
extern const Sprite *const asset_hero[HERO_SIZES];
extern const short asset_hero_height[HERO_SIZES];
extern const Sprite *const asset_hero_hat[HERO_SIZES];      /* [0] fermo, [1] testa abbassata */
extern const short asset_hero_full_w[HERO_SIZES];            /* larghezza prima del ritaglio */
extern const short asset_hero_ox[HERO_SIZES];                /* x del ritaglio nel disegno pieno */
extern const unsigned char asset_hero_head_down[HERO_FRAMES];
extern const unsigned char *const asset_gull;          /* 2 fotogrammi, 2 byte per riga */
extern const Bitmap asset_lantern[LANTERN_FRAMES];
extern const unsigned char asset_stars[STAR_COUNT][2];
extern const unsigned char *const asset_font[2];

#endif
