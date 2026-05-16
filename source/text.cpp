#include "text.h"

#include <string.h>
#include <SDL_ttf.h>

static const char *FONT_PATH = "romfs:/font.ttf";

// Small fixed set of pixel sizes the UI uses. Loaded once at startup.
struct FontSlot { int size; TTF_Font *font; };
static FontSlot g_fonts[8];
static int g_font_count = 0;

static TTF_Font *font_for_size(int size) {
    if (size < 8) size = 8;
    for (int i = 0; i < g_font_count; i++) {
        if (g_fonts[i].size == size) return g_fonts[i].font;
    }
    if (g_font_count >= (int)(sizeof(g_fonts) / sizeof(g_fonts[0]))) {
        // Reuse the largest slot rather than allocating unbounded sizes.
        return g_fonts[0].font;
    }
    TTF_Font *f = TTF_OpenFont(FONT_PATH, size);
    if (!f) {
        SDL_Log("TTF_OpenFont(%d): %s", size, TTF_GetError());
        return nullptr;
    }
    g_fonts[g_font_count++] = { size, f };
    return f;
}

// LRU-ish text texture cache. Key includes string, size, and color.
struct TextEntry {
    char text[16];
    int size;
    Uint32 color_rgba;
    SDL_Texture *tex;
    int w, h;
    Uint32 last_used;
};
constexpr int TEXT_CACHE_CAP = 96;
static TextEntry g_text_cache[TEXT_CACHE_CAP];
static int g_text_cache_count = 0;
static Uint32 g_text_tick = 0;

static Uint32 pack_color(SDL_Color c) {
    return ((Uint32)c.r << 24) | ((Uint32)c.g << 16) | ((Uint32)c.b << 8) | (Uint32)c.a;
}

static TextEntry *text_cache_get(SDL_Renderer *r, const char *text, int size, SDL_Color color) {
    Uint32 key_color = pack_color(color);
    for (int i = 0; i < g_text_cache_count; i++) {
        TextEntry &e = g_text_cache[i];
        if (e.size == size && e.color_rgba == key_color && strncmp(e.text, text, sizeof(e.text)) == 0) {
            e.last_used = ++g_text_tick;
            return &e;
        }
    }

    TTF_Font *font = font_for_size(size);
    if (!font) return nullptr;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return nullptr;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    int w = surf->w, h = surf->h;
    SDL_FreeSurface(surf);
    if (!tex) return nullptr;

    int slot;
    if (g_text_cache_count < TEXT_CACHE_CAP) {
        slot = g_text_cache_count++;
    } else {
        slot = 0;
        for (int i = 1; i < TEXT_CACHE_CAP; i++) {
            if (g_text_cache[i].last_used < g_text_cache[slot].last_used) slot = i;
        }
        if (g_text_cache[slot].tex) SDL_DestroyTexture(g_text_cache[slot].tex);
    }
    TextEntry &e = g_text_cache[slot];
    strncpy(e.text, text, sizeof(e.text) - 1);
    e.text[sizeof(e.text) - 1] = 0;
    e.size = size;
    e.color_rgba = key_color;
    e.tex = tex;
    e.w = w;
    e.h = h;
    e.last_used = ++g_text_tick;
    return &e;
}

void draw_text_centered(SDL_Renderer *r, int cx, int cy, int size,
                        const char *text, SDL_Color color, bool flip) {
    TextEntry *e = text_cache_get(r, text, size, color);
    if (!e) return;
    SDL_Rect dst = { cx - e->w / 2, cy - e->h / 2, e->w, e->h };
    SDL_RenderCopyEx(r, e->tex, nullptr, &dst, flip ? 180.0 : 0.0, nullptr, SDL_FLIP_NONE);
}

void text_shutdown() {
    for (int i = 0; i < g_text_cache_count; i++) {
        if (g_text_cache[i].tex) SDL_DestroyTexture(g_text_cache[i].tex);
    }
    g_text_cache_count = 0;
    for (int i = 0; i < g_font_count; i++) {
        if (g_fonts[i].font) TTF_CloseFont(g_fonts[i].font);
    }
    g_font_count = 0;
}
