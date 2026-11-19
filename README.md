# ZMK config for the Boardsource Rechteck (`bs_lp_60`)

Personal [ZMK](https://zmk.dev) firmware config for the [Boardsource Rechteck](https://boardsource.xyz/products/Rechteck), a wireless 60% keyboard. It started from Boardsource's [config template](https://github.com/boardsource/Rechtek_zmk_config) and has since grown a custom keymap, a battery-level fix and LED feedback.

## Build and flash

1. Every push and pull request runs the **Build** workflow in GitHub Actions.
2. Open the run and download the artifact named `firmware_<first 7 characters of the commit SHA>`. Unzip it to get `bs_lp_60-zmk.uf2`.
3. Plug the keyboard in over USB and **double-tap the reset button** (under the spacebar). A drive appears.
4. Drag `bs_lp_60-zmk.uf2` onto that drive. The keyboard restarts on its own.

You can also enter the bootloader with a key combo, see [Keymap](#keymap).

## ZMK version

`config/west.yml` pins ZMK to **v0.3** (Zephyr 3.5), and `.github/workflows/build.yml` calls ZMK's v0.3 build workflow. The board definition from Boardsource (`boardsource/Rechtek-zmk-config`, which is not pinned) uses the old board format and does not build on ZMK `main` (Zephyr 4.1). Moving to a newer ZMK means converting the board to the new format first.

## Keymap

The keymap is `config/bs_lp_60.keymap`, and the layer diagrams are in its comments. Summary:

| Layer | Purpose | How to reach it |
|---|---|---|
| Base | Normal typing | default |
| Function | F1 to F12, arrows, Home/End, PgUp/PgDn, Insert, Print Screen | hold `Fn` (second from the right on the bottom row) or `Caps`; double-tap to toggle |
| Control | Brightness, volume, mute, media keys | hold `Fn`, then hold the rightmost bottom key |
| System | Bluetooth profiles | hold `Fn`, then hold the bottom-row key where Right Gui is |

The top-left key on every non-base layer returns to Base. Right Shift turns Backspace into Delete, and `Caps` into Caps Lock.

**Key combos** (all four keys together, within 150 ms, on any layer):
- `Caps` + `LShift` + `RAlt` + `Backspace`: bootloader (flash mode)
- `Caps` + `LShift` + `RAlt` + `Esc`: reset (restart the keyboard)

## Bluetooth

The System layer has `BT0` to `BT4` (profiles 0 to 4) and `BtClr` (clear the current profile's pairing). The keyboard shows up as `bs_lp_60`.

ZMK saves the active profile **60 seconds** after you change it. If you reset or power the keyboard off sooner, it comes back on the previously saved profile.

## LED feedback

The blue LED is driven by a small module in this repo (`src/ble_profile_led.c`, options in `Kconfig`).

| Event | Pattern |
|---|---|
| Boot, reset or wake from deep sleep | One long flash (1 s) |
| Switching to Bluetooth profile 0, 1, 2, 3, 4 | 1, 2, 3, 4, 5 short flashes (150 ms on, 150 ms off) |
| Reconnecting to the same profile | none |

After a boot flash, the short profile flashes follow a moment later, so you see the keyboard start and which profile it is on. Timings are set by `CONFIG_BLE_PROFILE_LED_FLASH_MS`, `CONFIG_BLE_PROFILE_LED_BOOT_FLASH` and `CONFIG_BLE_PROFILE_LED_BOOT_MS`.

## Battery level

Boardsource's board file reads the battery on a pin that is also the external-power control pin, so ZMK reported 0%. `config/bs_lp_60.overlay` points it at the pin that works on this keyboard (P0.04), and the notes are in that file. The percentage is ZMK's rough linear estimate between 3.45 V (0%) and 4.2 V (100%). The resistor values in the divider are Boardsource's and have not been confirmed.

## Repository layout

| Path | What it is |
|---|---|
| `config/bs_lp_60.keymap` | The keymap |
| `config/bs_lp_60.overlay` | Devicetree override for the battery pin |
| `config/bs_lp_60.json` | Physical key layout, used by keymap tools |
| `config/west.yml` | ZMK and board dependencies (ZMK pinned to v0.3) |
| `src/`, `CMakeLists.txt`, `Kconfig`, `zephyr/module.yml` | The LED module |
| `build.yaml` | What the workflow builds (`bs_lp_60`) |
| `.github/workflows/build.yml` | Build workflow; names the artifact after the commit |

## Known limits

- The yellow power LED can't be controlled from firmware as far as I can tell: it is probably wired straight to the power supply or charger. Switching the `ext-power` pin (P0.31) off for a few seconds had no visible effect, and what that pin controls is unknown.
- There is no brightness control for the blue LED; it is fully on or off.
- The board definition comes from Boardsource's repo and is unpinned (`main`), so changes there could affect builds.
