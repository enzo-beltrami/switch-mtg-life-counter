#include "input.h"

#include <SDL.h>
#include "geometry.h"

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

void handle_tap(State &s, int W, int H, int sx, int sy) {
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
