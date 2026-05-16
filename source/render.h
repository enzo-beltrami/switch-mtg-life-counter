#pragma once

#include <SDL.h>
#include "state.h"

void draw_select_screen(SDL_Renderer *r, int W, int H);

void draw_tile_life  (SDL_Renderer *r, const Tile &t, const Player &p, SDL_Color color);
void draw_tile_cmd   (SDL_Renderer *r, const Tile &t, const Player &p, int self_idx, int num_players);
void draw_cmd_toggle (SDL_Renderer *r, const Tile &t, bool active);

void draw_reset         (SDL_Renderer *r, int W, int H);
void draw_reset_confirm (SDL_Renderer *r, int W, int H);
