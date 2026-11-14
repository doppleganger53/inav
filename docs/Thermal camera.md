# JS-MINI256 thermal camera control

INAV can control the confirmed UART functions of a JS-MINI256-series 256x192 analog thermal camera on the HGLRCF722 target. The analog CVBS video connection remains independent; this driver controls only the camera UART.

Supported controls are:

*   GVAR 0: palette selection, using either the four-palette list or all 15 confirmed palettes
*   GVAR 1: manual FFC event counter
*   GVAR 2: image-preset selector
*   GVAR 3: manual background-correction event counter

Digital zoom is not supported. Palette and image-preset changes are runtime-only. The driver does not automatically invoke Save Settings or write the camera's nonvolatile memory.

## Enable the driver

The thermal camera and RunCam protocols share INAV's existing RCDEVICE serial function and are mutually exclusive on a given firmware instance. RunCam remains the default.

In the CLI:

```
set thermal_camera_enabled = ON
set thermal_camera_full_palette = OFF
gvar 0 0 0 4
gvar 1 0 0 32767
gvar 2 0 0 3
gvar 3 0 0 32767
save
```

The default limited mode maps GVAR 0 as `0` White Hot, `1` Black Hot, `2` Iron Red 1, and `3` Fusion 1. The maximum of `4` allows the Programming Framework example below to increment and wrap the value in two transpiler-compatible steps; the final stored value remains in the range 0 through 3. To expose every confirmed palette instead, use:

```
set thermal_camera_full_palette = ON
gvar 0 0 0 28
save
```

The maximum of `28` is required by the bidirectional example below. Its decrement path implements `(GVAR 0 + 14) % 15`, so a current value of 14 temporarily becomes 28 before wrapping to 13. The Programming Framework finishes all rows before the thermal-camera task can read the GVAR.

In full-palette mode, GVAR 0 maps directly to the camera palette code:

| GVAR 0 | Palette |
| --- | --- |
| 0 | White Hot |
| 1 | Black Hot |
| 2 | Fusion 1 |
| 3 | Rainbow |
| 4 | Fusion 2 |
| 5 | Iron Red 1 |
| 6 | Iron Red 2 |
| 7 | Dark Brown |
| 8 | Color 1 |
| 9 | Color 2 |
| 10 | Ice/Fire |
| 11 | Rain |
| 12 | Green Hot |
| 13 | Red Hot |
| 14 | Dark Blue |

GVAR 2 selects one of four fixed runtime image presets:

| GVAR 2 | Preset | Brightness | Contrast | Enhancement | Static denoise | Dynamic denoise |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | Balanced | 50 | 50 | 50 | 50 | 50 |
| 1 | High detail | 50 | 55 | 70 | 30 | 30 |
| 2 | Low noise | 50 | 50 | 40 | 70 | 70 |
| 3 | High contrast | 50 | 70 | 60 | 45 | 45 |

Each preset is sent as five commands in table order, one command every 250 milliseconds. If GVAR 2 changes before a sequence finishes, the unfinished sequence is replaced by the newer selection. Unsupported values are clamped to the nearest preset. Presets are not automatically saved to camera nonvolatile memory and may not persist across a camera power cycle.

In the Ports tab, select **Runcam device** in the Peripherals column for one unused hardware UART. Do not select a baud rate; this function opens the UART internally at 115200 baud, 8 data bits, no parity, and 1 stop bit.

UART1 is the default serial receiver on HGLRCF722. Select a genuinely unused UART and confirm its pads against the board silkscreen or manual.

## Wiring

*   Camera TX to the selected FC UART RX
*   Camera RX to the selected FC UART TX
*   Camera GND to FC GND

Both sides use 3.3 V UART logic. CVBS wiring is separate from this control connection.

On the physically verified HGLRCF722 installation, UART6 is used: the camera blue RX wire connects to FC TX6, and the camera green TX wire connects to FC RX6.

## Programming Framework example

JavaScript is transpiled by INAV Configurator into Logic Conditions; it does not execute as JavaScript on the flight controller. Receiver channels use their one-based channel numbers. This example uses receiver channel 7 for palette cycling, channel 8 for FFC, channel 9 for preset cycling, and channel 10 for background correction. Replace the channel numbers to match the receiver setup.

```javascript
inav.events.edge(() => inav.rc[7].high, { duration: 0 }, () => {
  inav.gvar[0] = inav.gvar[0] + 1;
  inav.gvar[0] = inav.gvar[0] % 4;
});

inav.events.edge(() => inav.rc[8].high, { duration: 0 }, () => {
  inav.gvar[1] = inav.gvar[1] + 1;
  inav.gvar[1] = inav.gvar[1] % 32767;
});

inav.events.edge(() => inav.rc[9].high, { duration: 0 }, () => {
  inav.gvar[2] = inav.gvar[2] < 3 ? inav.gvar[2] + 1 : 0;
});

inav.events.edge(() => inav.rc[10].high, { duration: 0 }, () => {
  inav.gvar[3] = inav.gvar[3] + 1;
  inav.gvar[3] = inav.gvar[3] % 32767;
});
```

That example cycles the default four-palette list. With `thermal_camera_full_palette = ON`, use the following actions to increment on channel 15 high and decrement on channel 15 low:

```javascript
inav.events.edge(() => inav.rc[15].high, { duration: 0 }, () => {
  inav.gvar[0] = inav.gvar[0] + 1;
  inav.gvar[0] = inav.gvar[0] % 15;
});

inav.events.edge(() => inav.rc[15].low, { duration: 0 }, () => {
  inav.gvar[0] = inav.gvar[0] + 14;
  inav.gvar[0] = inav.gvar[0] % 15;
});
```

Use `.low` instead of `.high` if a switch is active below 1333 microseconds. Each `edge()` action runs once when its condition becomes true and rearms after it becomes false.

Keep each increment and modulo in separate assignments. INAV Configurator 9.1.x cannot transpile a nested assignment such as `inav.gvar[0] = (inav.gvar[0] + 1) % 15` and reports `Unknown operand ''`. Logic Conditions run in row order, so the second assignment wraps the value set by the first before the thermal-camera task reads it. GVAR 2 uses a conditional instead because its configured maximum is 3; each GVAR assignment is clamped immediately, so incrementing 3 before applying modulo would not wrap.

Treat the pasted JavaScript as the canonical source. INAV Configurator 9.1.x cannot losslessly decompile these staged GVAR operations: **Load from FC** rewrites them as nested ternary expressions with duplicated `edge()` calls. Do not save or transpile that decompiled form. Clear the editor and paste the canonical source again before making or saving later changes.

At startup the driver caches all four GVAR values and deliberately sends no command. Set the GVAR 0 default to the logical palette that the camera normally uses at boot and GVAR 2 to the desired starting preset. The first control event advances from those cached values. GVAR 1 and GVAR 3 use explicit modulo because GVAR writes otherwise clamp at their configured maximum rather than overflowing.

## Propeller-free bench test

1.  Remove all propellers, leave all momentary controls inactive, and power the camera using its documented supply.
2.  Configure the selected UART and enable the driver as above, then reboot the flight controller.
3.  For the verified UART6 installation, confirm blue camera RX is connected to FC TX6 and green camera TX is connected to FC RX6.
4.  Confirm that reboot alone does not change the palette or preset and does not trigger FFC or background correction.
5.  With GVAR 0 starting at White Hot (`0`), confirm successive presses send Black Hot, Iron Red 1, Fusion 1, White Hot, and repeat.
6.  Operate the FFC switch once and confirm exactly one brief shutter correction per press.
7.  Cycle through all four GVAR 2 presets. Allow at least 1.25 seconds for each five-command sequence to finish and confirm the final image matches the preset table. A logic analyzer on TX6 is the reliable way to verify command order and 250 millisecond pacing.
8.  Start one preset, select a different preset before 1.25 seconds elapses, and confirm the final image matches only the newer selection.
9.  Operate the GVAR 3 control once and confirm exactly one background correction per press. Hold each release for at least 200 milliseconds before pressing again, then confirm that it rearms and generates one new event.
10.  Power-cycle the camera and confirm the test does not depend on preset persistence; the driver does not automatically save presets to camera nonvolatile memory.

For full-palette mode, repeat the palette test in both directions and confirm all 15 table entries appear in order. GVAR 0 may momentarily reach its configured maximum of 28 within the Programming Framework task, but the following modulo row immediately restores it to the range 0 through 14 before the thermal-camera task reads it.

Receiving and validating camera acknowledgements is not implemented. A visible camera reaction confirms behavior during this initial bench test.