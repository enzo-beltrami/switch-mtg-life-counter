#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <switch.h>
#include <SDL.h>
#include <SDL_ttf.h>

constexpr int MAX_PLAYERS = 6;
constexpr int MIN_PLAYERS = 2;
constexpr int START_LIFE  = 40;
constexpr int WIN_W       = 1280;
constexpr int WIN_H       = 720;
constexpr Uint32 DELTA_WINDOW_MS = 5000;

enum Screen   { SCREEN_SELECT, SCREEN_GAME };
enum TileMode { MODE_LIFE, MODE_CMD };

struct Player {
    int      life;
    int      cmd_damage[MAX_PLAYERS];
    TileMode mode;
    int      recent_delta;
    Uint32   recent_delta_at;
};

struct Tile {
    int  x, y, w, h;
    bool rotated;
};

struct State {
    Screen screen;
    int    num_players;
    Player players[MAX_PLAYERS];
    Tile   tiles[MAX_PLAYERS];
    bool   reset_confirm;
    bool   quit;
};

static const SDL_Color player_colors[MAX_PLAYERS] = {
    {220,  60,  60, 255}, // red
    { 70, 130, 230, 255}, // blue
    {240, 140,  40, 255}, // orange
    {230, 200,  50, 255}, // yellow
    {180,  90, 210, 255}, // purple
    {150,  95,  50, 255}, // brown
};

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void reset_state(State &s) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        s.players[i].life = START_LIFE;
        s.players[i].mode = MODE_LIFE;
        s.players[i].recent_delta = 0;
        s.players[i].recent_delta_at = 0;
        for (int j = 0; j < MAX_PLAYERS; j++) {
            s.players[i].cmd_damage[j] = 0;
        }
    }
    s.reset_confirm = false;
}

static void layout_tiles(State &s, int W, int H) {
    int n = s.num_players;
    int top_n, bot_n;
    switch (n) {
        case 2: top_n = 1; bot_n = 1; break;
        case 3: top_n = 1; bot_n = 2; break;
        case 4: top_n = 2; bot_n = 2; break;
        case 5: top_n = 2; bot_n = 3; break;
        case 6: top_n = 3; bot_n = 3; break;
        default: top_n = 2; bot_n = 2; break;
    }
    int half_h = H / 2;
    for (int i = 0; i < top_n; i++) {
        int x0 = (i * W) / top_n;
        int x1 = ((i + 1) * W) / top_n;
        s.tiles[i] = { x0, 0, x1 - x0, half_h, true };
    }
    for (int i = 0; i < bot_n; i++) {
        int x0 = (i * W) / bot_n;
        int x1 = ((i + 1) * W) / bot_n;
        s.tiles[top_n + i] = { x0, half_h, x1 - x0, H - half_h, false };
    }
}

// ---- coordinate transforms ----

static SDL_Rect tile_to_screen(const Tile &t, SDL_Rect local) {
    SDL_Rect r;
    if (t.rotated) {
        r.x = t.x + t.w - local.x - local.w;
        r.y = t.y + t.h - local.y - local.h;
    } else {
        r.x = t.x + local.x;
        r.y = t.y + local.y;
    }
    r.w = local.w;
    r.h = local.h;
    return r;
}

static bool screen_to_tile_local(const Tile &t, int sx, int sy, int &lx, int &ly) {
    if (sx < t.x || sx >= t.x + t.w || sy < t.y || sy >= t.y + t.h) return false;
    int dx = sx - t.x;
    int dy = sy - t.y;
    if (t.rotated) {
        lx = t.w - 1 - dx;
        ly = t.h - 1 - dy;
    } else {
        lx = dx;
        ly = dy;
    }
    return true;
}

// ---- TTF text rendering ----

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

static void text_cache_clear() {
    for (int i = 0; i < g_text_cache_count; i++) {
        if (g_text_cache[i].tex) SDL_DestroyTexture(g_text_cache[i].tex);
    }
    g_text_cache_count = 0;
}

// Draws `text` centered at (cx, cy). When flip=true the texture is rotated 180°
// so a player on the opposite side of the screen reads it right-side-up.
static void draw_text_centered(SDL_Renderer *r, int cx, int cy, int size,
                               const char *text, SDL_Color color, bool flip) {
    TextEntry *e = text_cache_get(r, text, size, color);
    if (!e) return;
    SDL_Rect dst = { cx - e->w / 2, cy - e->h / 2, e->w, e->h };
    SDL_RenderCopyEx(r, e->tex, nullptr, &dst, flip ? 180.0 : 0.0, nullptr, SDL_FLIP_NONE);
}

// ---- tap-zone hint primitives (kept as plain rects — these are icons, not text) ----

static void draw_minus(SDL_Renderer *r, int x, int y, int W, int H, SDL_Color c) {
    int t = W / 4; if (t < 2) t = 2;
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_Rect rect = { x, y + H / 2 - t / 2, W, t };
    SDL_RenderFillRect(r, &rect);
}

static void draw_plus(SDL_Renderer *r, int x, int y, int W, int H, SDL_Color c) {
    int t = W / 4; if (t < 2) t = 2;
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_Rect h = { x, y + H / 2 - t / 2, W, t };
    SDL_Rect v = { x + W / 2 - t / 2, y, t, H };
    SDL_RenderFillRect(r, &h);
    SDL_RenderFillRect(r, &v);
}

// ---- layout constants inside a tile (local, unrotated coords) ----

static SDL_Rect tile_cmd_btn_local(const Tile &t) {
    int s = t.w / 10; if (s < 56) s = 56; if (s > 80) s = 80;
    return { 12, 12, s, s };
}

// ---- tile drawing ----

static void fill_color(SDL_Renderer *r, SDL_Color c, Uint8 mult) {
    SDL_SetRenderDrawColor(r, c.r * mult / 255, c.g * mult / 255, c.b * mult / 255, 255);
}

static void draw_tile_life(SDL_Renderer *r, const Tile &t, const Player &p, SDL_Color color) {
    fill_color(r, color, 220);
    SDL_Rect bg = { t.x + 6, t.y + 6, t.w - 12, t.h - 12 };
    SDL_RenderFillRect(r, &bg);

    int size = t.h / 2;
    if (size > 240) size = 240;
    if (size < 40) size = 40;
    int cx = t.x + t.w / 2;
    int cy = t.y + t.h / 2;
    SDL_Color white = {255, 255, 255, 255};
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", clampi(p.life, -99, 999));
    draw_text_centered(r, cx, cy, size, buf, white, t.rotated);

    // Underline below the number (in the player's reading direction) as an
    // orientation cue.
    int bar_w = size;
    int bar_h = size / 24; if (bar_h < 3) bar_h = 3; if (bar_h > 6) bar_h = 6;
    int bar_gap = 8;
    int bar_off = size / 2 + bar_gap;
    int bar_y = t.rotated ? (cy - bar_off - bar_h) : (cy + bar_off);
    SDL_Rect bar = { cx - bar_w / 2, bar_y, bar_w, bar_h };
    SDL_SetRenderDrawColor(r, 230, 230, 230, 255);
    SDL_RenderFillRect(r, &bar);

    Uint32 now = SDL_GetTicks();
    if (p.recent_delta != 0 && (now - p.recent_delta_at) <= DELTA_WINDOW_MS) {
        int dsize = size / 3;
        if (dsize < 18) dsize = 18;
        int dx_off = size / 2 + dsize;
        char dbuf[8];
        snprintf(dbuf, sizeof(dbuf), "%+d", p.recent_delta);
        SDL_Color dcol = p.recent_delta > 0
            ? SDL_Color{180, 240, 180, 255}
            : SDL_Color{245, 180, 180, 255};
        // Place above the main number in the player's reading direction.
        int dcy = t.rotated ? (cy + dx_off) : (cy - dx_off);
        draw_text_centered(r, cx, dcy, dsize, dbuf, dcol, t.rotated);
    }

    // Tap-zone hints: minus on local-left, plus on local-right.
    int ico = t.h / 8;
    if (ico < 32) ico = 32;
    if (ico > 64) ico = 64;
    int pad = ico / 2 + 16;
    SDL_Color hint = {200, 200, 200, 255};
    SDL_Rect minus_local = { pad, t.h/2 - ico/2, ico, ico };
    SDL_Rect plus_local  = { t.w - pad - ico, t.h/2 - ico/2, ico, ico };
    SDL_Rect minus_screen = tile_to_screen(t, minus_local);
    SDL_Rect plus_screen  = tile_to_screen(t, plus_local);
    draw_minus(r, minus_screen.x, minus_screen.y, ico, ico, hint);
    draw_plus(r,  plus_screen.x,  plus_screen.y,  ico, ico, hint);
}

static void draw_tile_cmd(SDL_Renderer *r, const Tile &t, const Player &p, int self_idx, int num_players) {
    fill_color(r, {30, 30, 30, 255}, 255);
    SDL_Rect bg = { t.x + 6, t.y + 6, t.w - 12, t.h - 12 };
    SDL_RenderFillRect(r, &bg);

    int opp_count = num_players - 1;
    if (opp_count < 1) return;
    int oi = 0;
    for (int i = 0; i < num_players; i++) {
        if (i == self_idx) continue;
        int y0 = (oi * t.h) / opp_count;
        int y1 = ((oi + 1) * t.h) / opp_count;
        SDL_Rect local = { 4, y0 + 4, t.w - 8, (y1 - y0) - 8 };
        SDL_Rect screen = tile_to_screen(t, local);
        fill_color(r, player_colors[i], 230);
        SDL_RenderFillRect(r, &screen);

        int size = local.h * 3 / 4;
        if (size > 96) size = 96;
        if (size < 20) size = 20;
        int cx = screen.x + screen.w / 2;
        int cy = screen.y + screen.h / 2;
        SDL_Color white = {255, 255, 255, 255};
        int dmg = clampi(p.cmd_damage[i], 0, 99);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", dmg);
        draw_text_centered(r, cx, cy, size, buf, white, t.rotated);

        int ico = local.h / 3;
        if (ico < 16) ico = 16;
        if (ico > 48) ico = 48;
        int pad = ico / 2 + 8;
        SDL_Color hint = {240, 240, 240, 255};
        SDL_Rect minus_local = { local.x + pad, local.y + local.h/2 - ico/2, ico, ico };
        SDL_Rect plus_local  = { local.x + local.w - pad - ico, local.y + local.h/2 - ico/2, ico, ico };
        SDL_Rect minus_screen = tile_to_screen(t, minus_local);
        SDL_Rect plus_screen  = tile_to_screen(t, plus_local);
        draw_minus(r, minus_screen.x, minus_screen.y, ico, ico, hint);
        draw_plus(r,  plus_screen.x,  plus_screen.y,  ico, ico, hint);
        oi++;
    }
}

static void draw_cmd_toggle(SDL_Renderer *r, const Tile &t, bool active) {
    SDL_Rect local  = tile_cmd_btn_local(t);
    SDL_Rect screen = tile_to_screen(t, local);
    if (active) {
        SDL_SetRenderDrawColor(r, 255, 200, 60, 255);
    } else {
        SDL_SetRenderDrawColor(r, 90, 90, 90, 255);
    }
    SDL_RenderFillRect(r, &screen);

    SDL_Color fg = active ? SDL_Color{30, 30, 30, 255} : SDL_Color{230, 230, 230, 255};
    int size = local.h * 3 / 4;
    int cx = screen.x + screen.w / 2;
    int cy = screen.y + screen.h / 2;
    draw_text_centered(r, cx, cy, size, "C", fg, t.rotated);
}

// ---- reset button ----

static SDL_Rect reset_rect(int W, int H) {
    (void)W; (void)H;
    int s = 70;
    return { 24, 24, s, s };
}

static void draw_reset(SDL_Renderer *r, int W, int H) {
    SDL_Rect rb = reset_rect(W, H);
    SDL_SetRenderDrawColor(r, 35, 35, 40, 255);
    SDL_RenderFillRect(r, &rb);
    SDL_SetRenderDrawColor(r, 230, 230, 230, 255);
    SDL_RenderDrawRect(r, &rb);
    draw_text_centered(r, rb.x + rb.w / 2, rb.y + rb.h / 2, 44, "R",
                       {220, 220, 220, 255}, false);
}

// ---- reset confirmation popup ----

static SDL_Rect confirm_box_rect(int W, int H) {
    int w = 560, h = 280;
    return { (W - w) / 2, (H - h) / 2, w, h };
}

static SDL_Rect confirm_no_rect(int W, int H) {
    SDL_Rect box = confirm_box_rect(W, H);
    int bw = 200, bh = 80;
    int gap = 40;
    int y = box.y + box.h - bh - 32;
    int x = box.x + box.w / 2 - bw - gap / 2;
    return { x, y, bw, bh };
}

static SDL_Rect confirm_yes_rect(int W, int H) {
    SDL_Rect box = confirm_box_rect(W, H);
    int bw = 200, bh = 80;
    int gap = 40;
    int y = box.y + box.h - bh - 32;
    int x = box.x + box.w / 2 + gap / 2;
    return { x, y, bw, bh };
}

static void draw_reset_confirm(SDL_Renderer *r, int W, int H) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 180);
    SDL_Rect full = { 0, 0, W, H };
    SDL_RenderFillRect(r, &full);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    SDL_Rect box = confirm_box_rect(W, H);
    SDL_SetRenderDrawColor(r, 40, 40, 50, 255);
    SDL_RenderFillRect(r, &box);
    SDL_SetRenderDrawColor(r, 220, 220, 230, 255);
    SDL_RenderDrawRect(r, &box);

    draw_text_centered(r, box.x + box.w / 2, box.y + 80, 40,
                       "Reset game?", {240, 240, 245, 255}, false);

    SDL_Rect no_btn  = confirm_no_rect(W, H);
    SDL_Rect yes_btn = confirm_yes_rect(W, H);

    SDL_SetRenderDrawColor(r, 70, 70, 80, 255);
    SDL_RenderFillRect(r, &no_btn);
    SDL_SetRenderDrawColor(r, 200, 200, 210, 255);
    SDL_RenderDrawRect(r, &no_btn);
    draw_text_centered(r, no_btn.x + no_btn.w / 2, no_btn.y + no_btn.h / 2,
                       36, "No", {240, 240, 245, 255}, false);

    SDL_SetRenderDrawColor(r, 180, 50, 50, 255);
    SDL_RenderFillRect(r, &yes_btn);
    SDL_SetRenderDrawColor(r, 240, 200, 200, 255);
    SDL_RenderDrawRect(r, &yes_btn);
    draw_text_centered(r, yes_btn.x + yes_btn.w / 2, yes_btn.y + yes_btn.h / 2,
                       36, "Yes", {255, 240, 240, 255}, false);
}

// ---- player select screen ----

static SDL_Rect quit_rect(int W, int H) {
    (void)H;
    int s = 80;
    return { W - s - 24, 24, s, s };
}

static SDL_Rect select_rect(int idx, int W, int H) {
    int btn = 150;
    int gap = 30;
    int total = 5 * btn + 4 * gap;
    int x0 = (W - total) / 2;
    int y0 = (H - btn) / 2;
    return { x0 + idx * (btn + gap), y0, btn, btn };
}

static void draw_select_screen(SDL_Renderer *r, int W, int H) {
    SDL_SetRenderDrawColor(r, 20, 20, 25, 255);
    SDL_Rect full = { 0, 0, W, H };
    SDL_RenderFillRect(r, &full);

    // Title bar
    SDL_SetRenderDrawColor(r, 60, 90, 140, 255);
    SDL_Rect title = { W/2 - 320, 80, 640, 60 };
    SDL_RenderFillRect(r, &title);
    draw_text_centered(r, title.x + title.w / 2, title.y + title.h / 2, 36,
                       "How many players?", {240, 240, 245, 255}, false);

    // Quit button (top-right)
    SDL_Rect qb = quit_rect(W, H);
    SDL_SetRenderDrawColor(r, 90, 30, 30, 255);
    SDL_RenderFillRect(r, &qb);
    SDL_SetRenderDrawColor(r, 230, 200, 200, 255);
    SDL_RenderDrawRect(r, &qb);
    draw_text_centered(r, qb.x + qb.w / 2, qb.y + qb.h / 2, 52, "X",
                       {240, 240, 240, 255}, false);

    SDL_Color white = {255, 255, 255, 255};
    for (int i = 0; i < 5; i++) {
        int n = MIN_PLAYERS + i;
        SDL_Rect rb = select_rect(i, W, H);
        SDL_SetRenderDrawColor(r, 50, 50, 60, 255);
        SDL_RenderFillRect(r, &rb);
        SDL_SetRenderDrawColor(r, 200, 200, 210, 255);
        SDL_RenderDrawRect(r, &rb);

        char buf[4];
        snprintf(buf, sizeof(buf), "%d", n);
        draw_text_centered(r, rb.x + rb.w / 2, rb.y + rb.h / 2, 96, buf, white, false);
    }
}

// ---- input ----

static void apply_life_tap(State &s, int idx, int lx) {
    Tile &t = s.tiles[idx];
    Player &p = s.players[idx];
    int old_life = p.life;
    int delta = (lx < t.w / 2) ? -1 : +1;
    p.life = clampi(p.life + delta, -99, 999);
    int actual = p.life - old_life;
    if (actual == 0) return;

    Uint32 now = SDL_GetTicks();
    if (now - p.recent_delta_at > DELTA_WINDOW_MS) {
        p.recent_delta = 0;
    }
    p.recent_delta += actual;
    p.recent_delta_at = now;
}

static void apply_cmd_tap(State &s, int self_idx, int lx, int ly) {
    Tile &t = s.tiles[self_idx];
    int opp_count = s.num_players - 1;
    if (opp_count < 1) return;
    int oi = (ly * opp_count) / t.h;
    if (oi < 0) oi = 0;
    if (oi >= opp_count) oi = opp_count - 1;
    int source = -1;
    int count = 0;
    for (int i = 0; i < s.num_players; i++) {
        if (i == self_idx) continue;
        if (count == oi) { source = i; break; }
        count++;
    }
    if (source < 0) return;
    int delta = (lx < t.w / 2) ? -1 : +1;
    s.players[self_idx].cmd_damage[source] =
        clampi(s.players[self_idx].cmd_damage[source] + delta, 0, 99);
}

static bool point_in(const SDL_Rect &r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static void handle_tap(State &s, int W, int H, int sx, int sy) {
    if (s.screen == SCREEN_SELECT) {
        if (point_in(quit_rect(W, H), sx, sy)) {
            s.quit = true;
            return;
        }
        for (int i = 0; i < 5; i++) {
            SDL_Rect rb = select_rect(i, W, H);
            if (point_in(rb, sx, sy)) {
                s.num_players = MIN_PLAYERS + i;
                reset_state(s);
                layout_tiles(s, W, H);
                s.screen = SCREEN_GAME;
                return;
            }
        }
        return;
    }

    // Reset confirmation popup intercepts all taps when open.
    if (s.reset_confirm) {
        if (point_in(confirm_yes_rect(W, H), sx, sy)) {
            reset_state(s);
            s.screen = SCREEN_SELECT;
            return;
        }
        if (point_in(confirm_no_rect(W, H), sx, sy)) {
            s.reset_confirm = false;
        }
        return;
    }

    SDL_Rect rb = reset_rect(W, H);
    if (point_in(rb, sx, sy)) {
        s.reset_confirm = true;
        return;
    }

    for (int i = 0; i < s.num_players; i++) {
        int lx, ly;
        if (!screen_to_tile_local(s.tiles[i], sx, sy, lx, ly)) continue;

        SDL_Rect cmd_btn = tile_cmd_btn_local(s.tiles[i]);
        if (point_in(cmd_btn, lx, ly)) {
            s.players[i].mode = (s.players[i].mode == MODE_LIFE) ? MODE_CMD : MODE_LIFE;
            return;
        }

        if (s.players[i].mode == MODE_LIFE) {
            apply_life_tap(s, i, lx);
        } else {
            apply_cmd_tap(s, i, lx, ly);
        }
        return;
    }
}

// ---- main ----

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    SDL_Event event;
    SDL_Window *window;
    SDL_Renderer *renderer;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        SDL_Log("SDL_Init: %s\n", SDL_GetError());
        return -1;
    }

    if (R_FAILED(romfsInit())) {
        SDL_Log("romfsInit failed");
        SDL_Quit();
        return -1;
    }

    if (TTF_Init() < 0) {
        SDL_Log("TTF_Init: %s\n", TTF_GetError());
        romfsExit();
        SDL_Quit();
        return -1;
    }

    window = SDL_CreateWindow("mtg-life-counter", 0, 0, WIN_W, WIN_H, 0);
    if (!window) {
        SDL_Log("SDL_CreateWindow: %s\n", SDL_GetError());
        TTF_Quit();
        romfsExit();
        SDL_Quit();
        return -1;
    }

    renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        romfsExit();
        SDL_Quit();
        return -1;
    }

    SDL_JoystickOpen(0);

    State state{};
    state.screen = SCREEN_SELECT;
    state.num_players = 4;
    reset_state(state);

    int done = 0;
    while (!done) {
        int W, H;
        SDL_GetWindowSize(window, &W, &H);

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_FINGERDOWN: {
                    int sx = (int)(event.tfinger.x * W);
                    int sy = (int)(event.tfinger.y * H);
                    handle_tap(state, W, H, sx, sy);
                } break;

                case SDL_MOUSEBUTTONDOWN: {
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        handle_tap(state, W, H, event.button.x, event.button.y);
                    }
                } break;

                case SDL_JOYBUTTONDOWN:
                    if (event.jbutton.which == 0 && event.jbutton.button == 10) {
                        // (+) opens the reset modal in-game; on the select
                        // screen it still quits since there's nothing to reset.
                        if (state.screen == SCREEN_GAME) {
                            state.reset_confirm = !state.reset_confirm;
                        } else {
                            done = 1;
                        }
                    }
                    break;

                case SDL_QUIT:
                    done = 1;
                    break;

                default:
                    break;
            }
        }

        if (state.quit) {
            done = 1;
        }

        SDL_SetRenderDrawColor(renderer, 10, 10, 12, 255);
        SDL_RenderClear(renderer);

        if (state.screen == SCREEN_SELECT) {
            draw_select_screen(renderer, W, H);
        } else {
            layout_tiles(state, W, H);
            for (int i = 0; i < state.num_players; i++) {
                const Tile &t = state.tiles[i];
                if (state.players[i].mode == MODE_LIFE) {
                    draw_tile_life(renderer, t, state.players[i], player_colors[i]);
                } else {
                    draw_tile_cmd(renderer, t, state.players[i], i, state.num_players);
                }
                draw_cmd_toggle(renderer, t, state.players[i].mode == MODE_CMD);
            }
            draw_reset(renderer, W, H);
            if (state.reset_confirm) {
                draw_reset_confirm(renderer, W, H);
            }
        }

        SDL_RenderPresent(renderer);
    }

    text_cache_clear();
    for (int i = 0; i < g_font_count; i++) TTF_CloseFont(g_fonts[i].font);
    g_font_count = 0;

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    romfsExit();
    SDL_Quit();
    return 0;
}
