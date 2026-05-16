#pragma once

#include <SDL.h>

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

extern const SDL_Color player_colors[MAX_PLAYERS];

int  clampi(int v, int lo, int hi);
void reset_state(State &s);
void layout_tiles(State &s, int W, int H);
