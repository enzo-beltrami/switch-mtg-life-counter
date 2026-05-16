#include <switch.h>
#include <SDL.h>
#include <SDL_ttf.h>

#include "state.h"
#include "render.h"
#include "input.h"
#include "text.h"

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

    text_shutdown();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    romfsExit();
    SDL_Quit();
    return 0;
}
