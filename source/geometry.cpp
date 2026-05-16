#include "geometry.h"

bool point_in(const SDL_Rect &r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

SDL_Rect tile_to_screen(const Tile &t, SDL_Rect local) {
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

bool screen_to_tile_local(const Tile &t, int sx, int sy, int &lx, int &ly) {
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

SDL_Rect tile_cmd_btn_local(const Tile &t) {
    int s = t.w / 10; if (s < 56) s = 56; if (s > 80) s = 80;
    return { 12, 12, s, s };
}

SDL_Rect reset_rect(int W, int H) {
    (void)W; (void)H;
    int s = 70;
    return { 24, 24, s, s };
}

SDL_Rect quit_rect(int W, int H) {
    (void)H;
    int s = 80;
    return { W - s - 24, 24, s, s };
}

SDL_Rect select_rect(int idx, int W, int H) {
    int btn = 150;
    int gap = 30;
    int total = 5 * btn + 4 * gap;
    int x0 = (W - total) / 2;
    int y0 = (H - btn) / 2;
    return { x0 + idx * (btn + gap), y0, btn, btn };
}

SDL_Rect confirm_box_rect(int W, int H) {
    int w = 560, h = 280;
    return { (W - w) / 2, (H - h) / 2, w, h };
}

SDL_Rect confirm_no_rect(int W, int H) {
    SDL_Rect box = confirm_box_rect(W, H);
    int bw = 200, bh = 80;
    int gap = 40;
    int y = box.y + box.h - bh - 32;
    int x = box.x + box.w / 2 - bw - gap / 2;
    return { x, y, bw, bh };
}

SDL_Rect confirm_yes_rect(int W, int H) {
    SDL_Rect box = confirm_box_rect(W, H);
    int bw = 200, bh = 80;
    int gap = 40;
    int y = box.y + box.h - bh - 32;
    int x = box.x + box.w / 2 + gap / 2;
    return { x, y, bw, bh };
}
