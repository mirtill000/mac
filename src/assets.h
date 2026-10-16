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
#define HERO_FRAMES 16
#define HERO_W 56
#define HERO_H 100
#define FONT_W 7
#define FONT_H 14
#define FONT_FIRST 32
#define GLYPH_ELLIPSIS 0x85
#define GLYPH_TRIANGLE 0x86

extern const PackedBitmap asset_bg[BG_COUNT];
extern const Bitmap asset_icon_small[ITEM_ICON_COUNT];
extern const Bitmap asset_icon_big[ITEM_ICON_COUNT];
extern const unsigned char *const asset_icon_mask; /* ICON_SMALL x ICON_SMALL ciascuna */
extern const Sprite asset_hero[HERO_FRAMES];
extern const unsigned char *const asset_font[2];

#endif
