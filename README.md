# Sand Sandbox for Flipper Zero

A small 2D falling-sand sandbox for the Flipper Zero. Draw with sand, water and walls and watch them interact. Built with [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) against the [Momentum firmware](https://github.com/Next-Flip/Momentum-Firmware) SDK (`mntm-012`).

## Features

- 64×28 cell playfield rendered at 2×2 pixels per cell
- **Sand** piles up, slides down slopes and sinks through water
- **Water** falls, spreads sideways and levels out
- **Walls** block everything
- **Erase** tool and one-shot clear

## Controls

| Input | Action |
|---|---|
| D-pad | Move cursor |
| OK (short press) | Toggle paint mode (places material as you move) |
| OK (hold) | Cycle material: Sand → Water → Wall → Erase |
| Back (hold) | Clear the playfield |
| Back (short press) | Exit |

The bottom bar shows the current material and whether paint mode is on.

## Install (prebuilt)

Download `sand_sandbox.fap` from the [Releases](../../releases) page and copy it to `apps/Games/` on the Flipper's SD card (with qFlipper, the mobile app, or a card reader). It appears under **Apps → Games**.

## Build from source

Requires Python 3 and [ufbt](https://pypi.org/project/ufbt/).

```
pip install ufbt
python -m ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json
python -m ufbt
```

The result is `dist/sand_sandbox.fap`. With a Flipper connected over USB (and qFlipper closed), `python -m ufbt launch` builds, uploads and runs it.

> On Windows, if `ufbt` isn't recognised, use `python -m ufbt` as above, or add Python's `Scripts` folder to your PATH.

To target the stock firmware instead of Momentum, run `ufbt update` with no index URL.

## Project layout

```
application.fam   App manifest
sand_sandbox.c    Game source
```

## Notes

- Targets Momentum firmware; it uses only standard Flipper APIs (furi, gui, input), so it should also build against other firmwares' SDKs.
- Simulation runs at roughly 25 ticks per second.

## License

MIT. See [LICENSE](LICENSE).
