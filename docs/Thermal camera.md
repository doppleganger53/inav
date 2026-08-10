# JS-MINI256 thermal camera control

INAV can control the confirmed UART functions of a JS-MINI256-series 256x192 analog thermal camera on the HGLRCF722 target. The analog CVBS video connection remains independent; this driver controls only the camera UART.

Supported controls are:

*   GVAR 0: palette (`0` White Hot, `1` Black Hot, `2` Iron Red 1, `3` Fusion 1)
*   GVAR 1: manual FFC event counter
*   Internal commands for background correction and saving settings

Digital zoom is not supported. Palette changes are not automatically saved to the camera, avoiding unnecessary nonvolatile writes.

## Enable the driver

The thermal camera and RunCam protocols share INAV's existing RCDEVICE serial function and are mutually exclusive on a given firmware instance. RunCam remains the default.

In the CLI:

```
set thermal_camera_enabled = ON
gvar 0 0 0 3
gvar 1 0 0 32767
save
```

In the Ports tab, select **Runcam device** in the Peripherals column for one unused hardware UART. Do not select a baud rate; this function opens the UART internally at 115200 baud, 8 data bits, no parity, and 1 stop bit.

UART1 is the default serial receiver on HGLRCF722. Select a genuinely unused UART and confirm its pads against the board silkscreen or manual.

## Wiring

*   Camera TX to the selected FC UART RX
*   Camera RX to the selected FC UART TX
*   Camera GND to FC GND

Both sides use 3.3 V UART logic. CVBS wiring is separate from this control connection.

## Programming Framework example

JavaScript is transpiled by INAV Configurator into Logic Conditions; it does not execute as JavaScript on the flight controller. This example uses receiver channel 7 (zero-based JavaScript index 6) for palette cycling and channel 8 (index 7) for FFC. Replace the two indices to match the receiver setup.

```javascript
inav.events.edge(() => inav.rc[6].high, { duration: 0 }, () => {
  inav.gvar[0] = (inav.gvar[0] + 1) % 4;
});

inav.events.edge(() => inav.rc[7].high, { duration: 0 }, () => {
  inav.gvar[1] = (inav.gvar[1] + 1) % 32768;
});
```

Use `.low` instead of `.high` if a switch is active below 1333 microseconds. Each `edge()` action runs once when its condition becomes true and rearms after it becomes false.

At startup the driver caches both GVAR values and deliberately sends no command. Set the GVAR 0 default to the logical palette that the camera normally uses at boot. The first palette-switch event advances from that value. GVAR 1 uses explicit modulo because GVAR writes otherwise clamp at their configured maximum rather than overflowing.

## Propeller-free bench test

1.  Remove all propellers, leave both momentary controls inactive, and power the camera using its documented supply.
2.  Configure the selected UART and enable the driver as above, then reboot the flight controller.
3.  Confirm that reboot alone neither changes the palette nor triggers the shutter.
4.  With GVAR 0 starting at White Hot (`0`), confirm successive presses send Black Hot, Iron Red 1, Fusion 1, White Hot, and repeat.
5.  Operate the FFC switch once and confirm exactly one brief shutter correction per press.
6.  Hold each release for at least 200 milliseconds before pressing again, then confirm that it rearms and generates one new event.

Receiving and validating camera acknowledgements is not implemented. A visible camera reaction confirms behavior during this initial bench test.