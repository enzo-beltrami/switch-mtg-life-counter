#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <SDL.h>

constexpr int MAX_PLAYERS = 6;
constexpr int MIN_PLAYERS = 2;
constexpr int START_LIFE  = 40;
constexpr int WIN_W       = 1280;
constexpr int WIN_H       = 720;
constexpr Uint32 RESET_ARM_MS = 1500;

enum Screen   { SCREEN_SELECT, SCREEN_GAME };
enum TileMode { MODE_LIFE, MODE_CMD };

struct Player {
    int      life;
    int      cmd_damage[MAX_PLAYERS];
    TileMode mode;
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
    bool   reset_armed;
    Uint32 reset_armed_at;
    bool   quit;
};

static const SDL_Color player_colors[MAX_PLAYERS] = {
    {220,  60,  60, 255}, // red
    { 70, 130, 230, 255}, // blue
    { 60, 200,  90, 255}, // green
    {230, 200,  50, 255}, // yellow
    {180,  90, 210, 255}, // purple
    {240, 140,  40, 255}, // orange
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
        for (int j = 0; j < MAX_PLAYERS; j++) {
            s.players[i].cmd_damage[j] = 0;
        }
    }
    s.reset_armed = false;
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

// ---- 7-segment digit rendering ----

// segments: 0=a top, 1=b top-right, 2=c bottom-right, 3=d bottom,
//           4=e bottom-left, 5=f top-left, 6=g middle
static const bool digit_segs[10][7] = {
    {1,1,1,1,1,1,0}, // 0
    {0,1,1,0,0,0,0}, // 1
    {1,1,0,1,1,0,1}, // 2
    {1,1,1,1,0,0,1}, // 3
    {0,1,1,0,0,1,1}, // 4
    {1,0,1,1,0,1,1}, // 5
    {1,0,1,1,1,1,1}, // 6
    {1,1,1,0,0,0,0}, // 7
    {1,1,1,1,1,1,1}, // 8
    {1,1,1,1,0,1,1}, // 9
};

static SDL_Rect seg_rect(int seg, int W, int H, int t) {
    SDL_Rect r = {0,0,0,0};
    switch (seg) {
        case 0: r = { 0,       0,            W, t };           break; // a
        case 1: r = { W - t,   0,            t, H / 2 };       break; // b
        case 2: r = { W - t,   H / 2,        t, H - H / 2 };   break; // c
        case 3: r = { 0,       H - t,        W, t };           break; // d
        case 4: r = { 0,       H / 2,        t, H - H / 2 };   break; // e
        case 5: r = { 0,       0,            t, H / 2 };       break; // f
        case 6: r = { 0,       H/2 - t/2,    W, t };           break; // g
    }
    return r;
}

static void draw_digit(SDL_Renderer *r, int x, int y, int W, int H, int digit, bool flip, SDL_Color c) {
    if (digit < 0 || digit > 9) return;
    int t = W / 4;
    if (t < 2) t = 2;
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    for (int s = 0; s < 7; s++) {
        if (!digit_segs[digit][s]) continue;
        SDL_Rect rect = seg_rect(s, W, H, t);
        if (flip) {
            rect.x = W - rect.x - rect.w;
            rect.y = H - rect.y - rect.h;
        }
        rect.x += x;
        rect.y += y;
        SDL_RenderFillRect(r, &rect);
    }
}

// ---- tiny 5x7 bitmap font (only glyphs we need) ----

struct Glyph { char c; const char *rows[7]; };
static const Glyph FONT[] = {
    {' ', {"     ", "     ", "     ", "     ", "     ", "     ", "     "}},
    {'?', {".XXX.", "X...X", "....X", "...X.", "..X..", ".....", "..X.."}},
    {'A', {".XXX.", "X...X", "X...X", "XXXXX", "X...X", "X...X", "X...X"}},
    {'E', {"XXXXX", "X....", "X....", "XXX..", "X....", "X....", "XXXXX"}},
    {'H', {"X...X", "X...X", "X...X", "XXXXX", "X...X", "X...X", "X...X"}},
    {'L', {"X....", "X....", "X....", "X....", "X....", "X....", "XXXXX"}},
    {'M', {"X...X", "XX.XX", "X.X.X", "X.X.X", "X...X", "X...X", "X...X"}},
    {'N', {"X...X", "XX..X", "X.X.X", "X..XX", "X...X", "X...X", "X...X"}},
    {'O', {".XXX.", "X...X", "X...X", "X...X", "X...X", "X...X", ".XXX."}},
    {'P', {"XXXX.", "X...X", "X...X", "XXXX.", "X....", "X....", "X...."}},
    {'R', {"XXXX.", "X...X", "X...X", "XXXX.", "X.X..", "X..X.", "X...X"}},
    {'S', {".XXXX", "X....", "X....", ".XXX.", "....X", "....X", "XXXX."}},
    {'W', {"X...X", "X...X", "X...X", "X.X.X", "X.X.X", "XX.XX", "X...X"}},
    {'Y', {"X...X", "X...X", ".X.X.", "..X..", "..X..", "..X..", "..X.."}},
};

static const Glyph *find_glyph(char c) {
    for (size_t i = 0; i < sizeof(FONT) / sizeof(FONT[0]); i++) {
        if (FONT[i].c == c) return &FONT[i];
    }
    return nullptr;
}

static int text_pixel_width(const char *s, int pix, int gap) {
    int n = 0;
    while (s[n]) n++;
    if (n == 0) return 0;
    return n * 5 * pix + (n - 1) * gap;
}

static void draw_text(SDL_Renderer *r, int x, int y, int pix, int gap, const char *s, SDL_Color col) {
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
    while (*s) {
        char c = *s;
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        const Glyph *g = find_glyph(c);
        if (g) {
            for (int row = 0; row < 7; row++) {
                for (int colp = 0; colp < 5; colp++) {
                    if (g->rows[row][colp] == 'X') {
                        SDL_Rect p = { x + colp * pix, y + row * pix, pix, pix };
                        SDL_RenderFillRect(r, &p);
                    }
                }
            }
        }
        x += 5 * pix + gap;
        s++;
    }
}

// Draws an 'X' inside (x, y, W, H) using two diagonals rasterized as 1px strokes.
static void draw_letter_x(SDL_Renderer *r, int x, int y, int W, int H, SDL_Color c) {
    int t = W / 6; if (t < 2) t = 2;
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    int span = (H < W ? H : W) - 1;
    for (int i = 0; i <= span; i++) {
        int px1 = x + (i * (W - t)) / span;
        int py  = y + (i * (H - 1)) / span;
        int px2 = x + W - t - (i * (W - t)) / span;
        SDL_Rect a = { px1, py, t, 1 };
        SDL_Rect b = { px2, py, t, 1 };
        SDL_RenderFillRect(r, &a);
        SDL_RenderFillRect(r, &b);
    }
}

// Draws a capital 'R' inside (x, y, W, H). Not rotation-aware — used only on the
// center reset button. Diagonal leg is rasterized as 1px-tall rect strokes.
static void draw_letter_r(SDL_Renderer *r, int x, int y, int W, int H, SDL_Color c) {
    int t = W / 5; if (t < 2) t = 2;
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);

    SDL_Rect left = { x, y, t, H };
    SDL_RenderFillRect(r, &left);
    SDL_Rect top = { x, y, W, t };
    SDL_RenderFillRect(r, &top);
    SDL_Rect tr = { x + W - t, y, t, H / 2 };
    SDL_RenderFillRect(r, &tr);
    SDL_Rect mid = { x, y + H/2 - t/2, W, t };
    SDL_RenderFillRect(r, &mid);

    // Diagonal leg from inner-left of the bowl down to bottom-right.
    int sx = x + t;
    int sy = y + H/2 + t/2;
    int ex = x + W - t;
    int ey = y + H - 1;
    int dy = ey - sy;
    int dx = ex - sx;
    if (dy > 0) {
        for (int i = 0; i <= dy; i++) {
            int px = sx + (i * dx) / dy;
            SDL_Rect stroke = { px, sy + i, t, 1 };
            SDL_RenderFillRect(r, &stroke);
        }
    }
}

// Draws a 7-segment-style 'C' using segments a, d, e, f (same flip semantics as digits).
static void draw_letter_c(SDL_Renderer *r, int x, int y, int W, int H, bool flip, SDL_Color c) {
    int t = W / 4; if (t < 2) t = 2;
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    const int segs[] = { 0, 3, 4, 5 }; // a, d, e, f
    for (int i = 0; i < 4; i++) {
        SDL_Rect rect = seg_rect(segs[i], W, H, t);
        if (flip) {
            rect.x = W - rect.x - rect.w;
            rect.y = H - rect.y - rect.h;
        }
        rect.x += x;
        rect.y += y;
        SDL_RenderFillRect(r, &rect);
    }
}

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

// Draws an integer centered at (cx, cy). When flip=true, the whole number reads
// right-side-up to a viewer looking from the opposite side of the screen.
static void draw_number(SDL_Renderer *r, int cx, int cy, int dW, int dH, int value, bool flip, SDL_Color c) {
    char buf[8];
    int v = value;
    bool neg = v < 0;
    if (neg) v = -v;
    int n = snprintf(buf, sizeof(buf), "%d", v);
    int spacing = dW / 5;
    int sign_w  = neg ? (dW + spacing) : 0;
    int total_w = n * dW + (n - 1) * spacing + sign_w;
    int x = cx - total_w / 2;
    int y = cy - dH / 2;
    if (flip) {
        for (int i = n - 1; i >= 0; i--) {
            draw_digit(r, x, y, dW, dH, buf[i] - '0', true, c);
            x += dW + spacing;
        }
        if (neg) {
            draw_minus(r, x, y, dW, dH, c);
        }
    } else {
        if (neg) {
            draw_minus(r, x, y, dW, dH, c);
            x += dW + spacing;
        }
        for (int i = 0; i < n; i++) {
            draw_digit(r, x, y, dW, dH, buf[i] - '0', false, c);
            x += dW + spacing;
        }
    }
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
    fill_color(r, color, 70);
    SDL_Rect bg = { t.x + 6, t.y + 6, t.w - 12, t.h - 12 };
    SDL_RenderFillRect(r, &bg);

    int dW = t.h / 4;
    if (dW > 140) dW = 140;
    int dH = dW * 2;
    int cx = t.x + t.w / 2;
    int cy = t.y + t.h / 2;
    SDL_Color white = {255, 255, 255, 255};
    draw_number(r, cx, cy, dW, dH, clampi(p.life, -99, 999), t.rotated, white);

    // Tap-zone hints: minus on local-left, plus on local-right (rotation handled
    // by tile_to_screen).
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
        fill_color(r, player_colors[i], 180);
        SDL_RenderFillRect(r, &screen);

        int dH = local.h * 3 / 4;
        int dW = dH / 2;
        if (dW > 64) dW = 64;
        if (dW < 14) dW = 14;
        dH = dW * 2;
        int cx = screen.x + screen.w / 2;
        int cy = screen.y + screen.h / 2;
        SDL_Color white = {255, 255, 255, 255};
        int dmg = clampi(p.cmd_damage[i], 0, 99);
        draw_number(r, cx, cy, dW, dH, dmg, t.rotated, white);
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

    // 'C' glyph, centered inside the button, flipped on rotated tiles so it reads
    // right-side-up to the player.
    int gW = local.w / 2;
    int gH = (gW * 3) / 2;
    int glyph_lx = local.x + (local.w - gW) / 2;
    int glyph_ly = local.y + (local.h - gH) / 2;
    SDL_Rect glyph_local = { glyph_lx, glyph_ly, gW, gH };
    SDL_Rect glyph_screen = tile_to_screen(t, glyph_local);
    SDL_Color fg = active ? SDL_Color{30, 30, 30, 255} : SDL_Color{230, 230, 230, 255};
    draw_letter_c(r, glyph_screen.x, glyph_screen.y, gW, gH, t.rotated, fg);
}

// ---- reset button ----

static SDL_Rect reset_rect(int W, int H) {
    (void)W; (void)H;
    int s = 70;
    return { 24, 24, s, s };
}

static void draw_reset(SDL_Renderer *r, int W, int H, bool armed) {
    SDL_Rect rb = reset_rect(W, H);
    if (armed) {
        SDL_SetRenderDrawColor(r, 220, 60, 60, 255);
    } else {
        SDL_SetRenderDrawColor(r, 35, 35, 40, 255);
    }
    SDL_RenderFillRect(r, &rb);
    SDL_SetRenderDrawColor(r, 230, 230, 230, 255);
    SDL_RenderDrawRect(r, &rb);

    SDL_Color fg = armed ? SDL_Color{255, 255, 255, 255} : SDL_Color{220, 220, 220, 255};
    int gW = 28;
    int gH = gW * 3 / 2;
    int gx = rb.x + rb.w / 2 - gW / 2;
    int gy = rb.y + rb.h / 2 - gH / 2;
    draw_letter_r(r, gx, gy, gW, gH, fg);
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

    {
        const char *prompt = "HOW MANY PLAYERS?";
        int pix = 4;
        int gap = pix;
        int tw = text_pixel_width(prompt, pix, gap);
        int th = 7 * pix;
        int tx = title.x + (title.w - tw) / 2;
        int ty = title.y + (title.h - th) / 2;
        draw_text(r, tx, ty, pix, gap, prompt, {240, 240, 245, 255});
    }

    // Quit button (top-right)
    SDL_Rect qb = quit_rect(W, H);
    SDL_SetRenderDrawColor(r, 90, 30, 30, 255);
    SDL_RenderFillRect(r, &qb);
    SDL_SetRenderDrawColor(r, 230, 200, 200, 255);
    SDL_RenderDrawRect(r, &qb);
    {
        int inset = qb.w / 5;
        draw_letter_x(r, qb.x + inset, qb.y + inset,
                      qb.w - 2 * inset, qb.h - 2 * inset,
                      {240, 240, 240, 255});
    }

    SDL_Color white = {255, 255, 255, 255};
    for (int i = 0; i < 5; i++) {
        int n = MIN_PLAYERS + i;
        SDL_Rect rb = select_rect(i, W, H);
        SDL_SetRenderDrawColor(r, 50, 50, 60, 255);
        SDL_RenderFillRect(r, &rb);
        SDL_SetRenderDrawColor(r, 200, 200, 210, 255);
        SDL_RenderDrawRect(r, &rb);

        int dW = 70;
        int dH = dW * 2;
        int cx = rb.x + rb.w / 2 - dW / 2;
        int cy = rb.y + rb.h / 2 - dH / 2;
        draw_digit(r, cx, cy, dW, dH, n, false, white);
    }
}

// ---- input ----

static void apply_life_tap(State &s, int idx, int lx) {
    Tile &t = s.tiles[idx];
    int delta = (lx < t.w / 2) ? -1 : +1;
    s.players[idx].life = clampi(s.players[idx].life + delta, -99, 999);
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
    int new_dmg = clampi(s.players[self_idx].cmd_damage[source] + delta, 0, 99);
    int actual = new_dmg - s.players[self_idx].cmd_damage[source];
    s.players[self_idx].cmd_damage[source] = new_dmg;
    s.players[self_idx].life = clampi(s.players[self_idx].life - actual, -99, 999);
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

    // Reset button (with arm-to-confirm)
    SDL_Rect rb = reset_rect(W, H);
    if (point_in(rb, sx, sy)) {
        Uint32 now = SDL_GetTicks();
        if (s.reset_armed && (now - s.reset_armed_at) <= RESET_ARM_MS) {
            reset_state(s);
            s.screen = SCREEN_SELECT;
        } else {
            s.reset_armed = true;
            s.reset_armed_at = now;
        }
        return;
    }
    // Tapping anywhere else cancels arm timer naturally via timeout, but we
    // also clear it on any non-reset tap to avoid lingering red state.
    s.reset_armed = false;

    // Tile hit-test
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

    window = SDL_CreateWindow("mtg-life-counter", 0, 0, WIN_W, WIN_H, 0);
    if (!window) {
        SDL_Log("SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Keep joystick #0 open only so the (+) button can quit cleanly.
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
                    // SDL on Switch may also surface touch as mouse events; handle for safety.
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        handle_tap(state, W, H, event.button.x, event.button.y);
                    }
                } break;

                case SDL_JOYBUTTONDOWN:
                    if (event.jbutton.which == 0 && event.jbutton.button == 10) {
                        // (+) quits
                        done = 1;
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

        // Clear arm flag if timer elapsed
        if (state.reset_armed &&
            (SDL_GetTicks() - state.reset_armed_at) > RESET_ARM_MS) {
            state.reset_armed = false;
        }

        // Background
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
            draw_reset(renderer, W, H, state.reset_armed);
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
