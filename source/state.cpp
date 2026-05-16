#include "state.h"

const SDL_Color player_colors[MAX_PLAYERS] = {
    {220,  60,  60, 255}, // red
    { 70, 130, 230, 255}, // blue
    {240, 140,  40, 255}, // orange
    {230, 200,  50, 255}, // yellow
    {180,  90, 210, 255}, // purple
    {150,  95,  50, 255}, // brown
};

int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void reset_state(State &s) {
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

void layout_tiles(State &s, int W, int H) {
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
