#include "render.h"

#include <stdio.h>
#include "geometry.h"
#include "text.h"

static void fill_color(SDL_Renderer *r, SDL_Color c, Uint8 mult) {
    SDL_SetRenderDrawColor(r, c.r * mult / 255, c.g * mult / 255, c.b * mult / 255, 255);
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

// ---- tile drawing ----

void draw_tile_life(SDL_Renderer *r, const Tile &t, const Player &p, SDL_Color color) {
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

void draw_tile_cmd(SDL_Renderer *r, const Tile &t, const Player &p, int self_idx, int num_players) {
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

void draw_cmd_toggle(SDL_Renderer *r, const Tile &t, bool active) {
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

void draw_reset(SDL_Renderer *r, int W, int H) {
    SDL_Rect rb = reset_rect(W, H);
    SDL_SetRenderDrawColor(r, 35, 35, 40, 255);
    SDL_RenderFillRect(r, &rb);
    SDL_SetRenderDrawColor(r, 230, 230, 230, 255);
    SDL_RenderDrawRect(r, &rb);
    draw_text_centered(r, rb.x + rb.w / 2, rb.y + rb.h / 2, 44, "R",
                       {220, 220, 220, 255}, false);
}

// ---- reset confirmation popup ----

void draw_reset_confirm(SDL_Renderer *r, int W, int H) {
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

void draw_select_screen(SDL_Renderer *r, int W, int H) {
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
