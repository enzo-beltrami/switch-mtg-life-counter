# mtg-life-counter

<p align="center">
  <img src="icon.jpg" alt="mtg-life-counter icon" width="256" />
</p>

A touch-only Commander life counter for the Nintendo Switch, built with SDL2 and
devkitPro. Made for handheld / tabletop mode — controller input is ignored.

## Features

- Player-select screen for 2–6 players
- Top-row tiles render upside-down so opposing players read right-side-up
- Tap the **right half** of a tile to add life, the **left half** to subtract
- Per-tile **C** button toggles commander-damage mode: a stack of horizontal
  rows, one per opponent, each colored to match that opponent. Tap the
  **right half** of a row to add commander damage, the **left half** to
  subtract. Commander damage is tracked independently of life — life only
  changes from taps on the life screen.
- **R** button in the top-left of the game screen resets all life to 40 — opens
  a Yes/No confirmation popup to avoid accidents

## Build

See [CONTRIBUTING.md](CONTRIBUTING.md) for build, layout, and release
instructions.

## Install / run

Copy `mtg-life-counter.nro` to the `/switch/` directory on your Switch's SD card
and launch it from the Homebrew Menu. For iterative development, push the NRO
over the network with `nxlink`:

```sh
nxlink -a <switch-ip> mtg-life-counter.nro
```

