/*
 * This file is part of INAV Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the GNU General Public License Version 3, as described below:
 *
 * This file is free software: you may copy, redistribute and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see http://www.gnu.org/licenses/.
 */

#include "platform.h"

#if defined(USE_THERMAL_CAMERA) && (!defined(USE_RCDEVICE) || !defined(USE_PROGRAMMING_FRAMEWORK))
#error "USE_THERMAL_CAMERA requires USE_RCDEVICE and USE_PROGRAMMING_FRAMEWORK"
#endif

#ifdef USE_THERMAL_CAMERA

#include <stdbool.h>
#include <stdint.h>

#include "common/time.h"
#include "common/utils.h"

#include "config/parameter_group.h"
#include "config/parameter_group_ids.h"

#include "drivers/serial.h"

#include "io/serial.h"
#include "io/thermal_camera.h"

#include "programming/global_variables.h"

#define THERMAL_CAMERA_HEADER 0xF0
#define THERMAL_CAMERA_PAYLOAD_SIZE 0x05
#define THERMAL_CAMERA_DEVICE 0x36
#define THERMAL_CAMERA_TRAILER 0xFF

#define THERMAL_CAMERA_WRITE 0x00

#define THERMAL_CAMERA_CLASS_PALETTE 0x78
#define THERMAL_CAMERA_SUBCOMMAND_PALETTE 0x20
#define THERMAL_CAMERA_CLASS_IMAGE 0x78
#define THERMAL_CAMERA_SUBCOMMAND_BRIGHTNESS 0x02
#define THERMAL_CAMERA_SUBCOMMAND_CONTRAST 0x03
#define THERMAL_CAMERA_SUBCOMMAND_ENHANCEMENT 0x10
#define THERMAL_CAMERA_SUBCOMMAND_STATIC_DENOISE 0x15
#define THERMAL_CAMERA_SUBCOMMAND_DYNAMIC_DENOISE 0x16
#define THERMAL_CAMERA_CLASS_CORRECTION 0x7C
#define THERMAL_CAMERA_SUBCOMMAND_FFC 0x02
#define THERMAL_CAMERA_SUBCOMMAND_BACKGROUND_CORRECTION 0x03
#define THERMAL_CAMERA_CLASS_SETTINGS 0x74
#define THERMAL_CAMERA_SUBCOMMAND_SAVE_SETTINGS 0x10

#define THERMAL_CAMERA_FULL_PALETTE_MAX 0x0E
#define THERMAL_CAMERA_PRESET_MAX 3
#define THERMAL_CAMERA_PRESET_COMMAND_INTERVAL_US MS2US(250)

#define THERMAL_CAMERA_GVAR_PALETTE 0
#define THERMAL_CAMERA_GVAR_FFC 1
#define THERMAL_CAMERA_GVAR_PRESET 2
#define THERMAL_CAMERA_GVAR_BACKGROUND_CORRECTION 3

typedef struct thermalCameraImageCommand_s {
    uint8_t commandClass;
    uint8_t subcommand;
} thermalCameraImageCommand_t;

static const thermalCameraImageCommand_t thermalCameraImageCommands[THERMAL_CAMERA_IMAGE_SETTING_COUNT] = {
    [THERMAL_CAMERA_IMAGE_BRIGHTNESS] = { THERMAL_CAMERA_CLASS_IMAGE, THERMAL_CAMERA_SUBCOMMAND_BRIGHTNESS },
    [THERMAL_CAMERA_IMAGE_CONTRAST] = { THERMAL_CAMERA_CLASS_IMAGE, THERMAL_CAMERA_SUBCOMMAND_CONTRAST },
    [THERMAL_CAMERA_IMAGE_ENHANCEMENT] = { THERMAL_CAMERA_CLASS_IMAGE, THERMAL_CAMERA_SUBCOMMAND_ENHANCEMENT },
    [THERMAL_CAMERA_IMAGE_STATIC_DENOISE] = { THERMAL_CAMERA_CLASS_IMAGE, THERMAL_CAMERA_SUBCOMMAND_STATIC_DENOISE },
    [THERMAL_CAMERA_IMAGE_DYNAMIC_DENOISE] = { THERMAL_CAMERA_CLASS_IMAGE, THERMAL_CAMERA_SUBCOMMAND_DYNAMIC_DENOISE },
};

static const uint8_t thermalCameraImagePresets[][THERMAL_CAMERA_IMAGE_SETTING_COUNT] = {
    { 50, 50, 50, 50, 50 },
    { 50, 55, 70, 30, 30 },
    { 50, 50, 40, 70, 70 },
    { 50, 70, 60, 45, 45 },
};

void thermalCameraBuildPacket(uint8_t commandClass, uint8_t subcommand, uint8_t readWrite, uint8_t value,
    uint8_t packet[THERMAL_CAMERA_PACKET_SIZE])
{
    packet[0] = THERMAL_CAMERA_HEADER;
    packet[1] = THERMAL_CAMERA_PAYLOAD_SIZE;
    packet[2] = THERMAL_CAMERA_DEVICE;
    packet[3] = commandClass;
    packet[4] = subcommand;
    packet[5] = readWrite;
    packet[6] = value;
    packet[7] = (uint8_t)((THERMAL_CAMERA_DEVICE + commandClass + subcommand + readWrite + value) & 0xFF);
    packet[8] = THERMAL_CAMERA_TRAILER;
}

uint8_t thermalCameraPaletteCode(int32_t logicalPalette, bool fullPalette)
{
    static const uint8_t paletteCodes[] = { 0x00, 0x01, 0x05, 0x02 };

    if (fullPalette) {
        if (logicalPalette < 0) {
            return 0;
        }
        if (logicalPalette > THERMAL_CAMERA_FULL_PALETTE_MAX) {
            return THERMAL_CAMERA_FULL_PALETTE_MAX;
        }

        return (uint8_t)logicalPalette;
    }

    if (logicalPalette < 0) {
        logicalPalette = 0;
    } else if (logicalPalette >= (int32_t)ARRAYLEN(paletteCodes)) {
        logicalPalette = (int32_t)ARRAYLEN(paletteCodes) - 1;
    }

    return paletteCodes[logicalPalette];
}

uint8_t thermalCameraPresetIndex(int32_t logicalPreset)
{
    if (logicalPreset < 0) {
        return 0;
    }
    if (logicalPreset > THERMAL_CAMERA_PRESET_MAX) {
        return THERMAL_CAMERA_PRESET_MAX;
    }

    return (uint8_t)logicalPreset;
}

uint8_t thermalCameraPresetValue(int32_t logicalPreset, thermalCameraImageSetting_e setting)
{
    if ((unsigned)setting >= THERMAL_CAMERA_IMAGE_SETTING_COUNT) {
        return 0;
    }

    return thermalCameraImagePresets[thermalCameraPresetIndex(logicalPreset)][setting];
}

#ifndef THERMAL_CAMERA_UNIT_TEST

PG_REGISTER_WITH_RESET_TEMPLATE(thermalCameraConfig_t, thermalCameraConfig, PG_RCDEVICE_CONFIG, 2);

PG_RESET_TEMPLATE(thermalCameraConfig_t, thermalCameraConfig,
    .enabled = false,
    .fullPalette = false,
);
#endif

static serialPort_t *thermalCameraPort;
static bool thermalCameraFullPalette;
static int32_t lastPalette;
static int32_t lastFfcEvent;
static int32_t lastPreset;
static int32_t lastBackgroundCorrectionEvent;
static uint8_t nextPresetSetting;
static bool presetCommandIntervalActive;
static timeUs_t nextPresetCommandAt;

static void thermalCameraCacheControls(void)
{
    lastPalette = gvGet(THERMAL_CAMERA_GVAR_PALETTE);
    lastFfcEvent = gvGet(THERMAL_CAMERA_GVAR_FFC);
    lastPreset = gvGet(THERMAL_CAMERA_GVAR_PRESET);
    lastBackgroundCorrectionEvent = gvGet(THERMAL_CAMERA_GVAR_BACKGROUND_CORRECTION);
    nextPresetSetting = THERMAL_CAMERA_IMAGE_SETTING_COUNT;
    presetCommandIntervalActive = false;
    nextPresetCommandAt = 0;
}

static bool thermalCameraSendCommand(uint8_t commandClass, uint8_t subcommand, uint8_t value)
{
    if (!thermalCameraPort || serialTxBytesFree(thermalCameraPort) < THERMAL_CAMERA_PACKET_SIZE) {
        return false;
    }

    uint8_t packet[THERMAL_CAMERA_PACKET_SIZE];
    thermalCameraBuildPacket(commandClass, subcommand, THERMAL_CAMERA_WRITE, value, packet);
    serialWriteBuf(thermalCameraPort, packet, sizeof(packet));

    return true;
}

bool thermalCameraSetPalette(int32_t logicalPalette)
{
    return thermalCameraSendCommand(THERMAL_CAMERA_CLASS_PALETTE, THERMAL_CAMERA_SUBCOMMAND_PALETTE,
        thermalCameraPaletteCode(logicalPalette, thermalCameraFullPalette));
}

bool thermalCameraTriggerFfc(void)
{
    return thermalCameraSendCommand(THERMAL_CAMERA_CLASS_CORRECTION, THERMAL_CAMERA_SUBCOMMAND_FFC, 0);
}

bool thermalCameraTriggerBackgroundCorrection(void)
{
    return thermalCameraSendCommand(THERMAL_CAMERA_CLASS_CORRECTION,
        THERMAL_CAMERA_SUBCOMMAND_BACKGROUND_CORRECTION, 0);
}

bool thermalCameraSaveSettings(void)
{
    return thermalCameraSendCommand(THERMAL_CAMERA_CLASS_SETTINGS, THERMAL_CAMERA_SUBCOMMAND_SAVE_SETTINGS, 0);
}

static bool thermalCameraSetImageSetting(int32_t logicalPreset, thermalCameraImageSetting_e setting)
{
    const thermalCameraImageCommand_t *command = &thermalCameraImageCommands[setting];
    return thermalCameraSendCommand(command->commandClass, command->subcommand,
        thermalCameraPresetValue(logicalPreset, setting));
}

#ifndef THERMAL_CAMERA_UNIT_TEST

bool thermalCameraIsConfigured(void)
{
    return thermalCameraConfig()->enabled;
}

bool thermalCameraIsEnabled(void)
{
    return thermalCameraPort != NULL;
}

void thermalCameraInit(void)
{
    thermalCameraPort = NULL;
    thermalCameraFullPalette = thermalCameraConfig()->fullPalette;
    thermalCameraCacheControls();

    if (!thermalCameraIsConfigured()) {
        return;
    }

    const serialPortConfig_t *portConfig = findSerialPortConfig(FUNCTION_RCDEVICE);
    if (portConfig) {
        thermalCameraPort = openSerialPort(portConfig->identifier, FUNCTION_RCDEVICE, NULL, NULL,
            baudRates[BAUD_115200], MODE_RXTX, SERIAL_NOT_INVERTED);
    }
}
#else
void thermalCameraTestInit(bool fullPalette)
{
    thermalCameraPort = (serialPort_t *)(uintptr_t)1;
    thermalCameraFullPalette = fullPalette;
    thermalCameraCacheControls();
}
#endif

void thermalCameraUpdate(timeUs_t currentTimeUs)
{
    const int32_t palette = gvGet(THERMAL_CAMERA_GVAR_PALETTE);
    if (palette != lastPalette && thermalCameraSetPalette(palette)) {
        lastPalette = palette;
    }

    const int32_t ffcEvent = gvGet(THERMAL_CAMERA_GVAR_FFC);
    if (ffcEvent != lastFfcEvent && thermalCameraTriggerFfc()) {
        lastFfcEvent = ffcEvent;
    }

    const int32_t backgroundCorrectionEvent = gvGet(THERMAL_CAMERA_GVAR_BACKGROUND_CORRECTION);
    if (backgroundCorrectionEvent != lastBackgroundCorrectionEvent && thermalCameraTriggerBackgroundCorrection()) {
        lastBackgroundCorrectionEvent = backgroundCorrectionEvent;
    }

    const int32_t preset = gvGet(THERMAL_CAMERA_GVAR_PRESET);
    if (preset != lastPreset) {
        lastPreset = preset;
        nextPresetSetting = THERMAL_CAMERA_IMAGE_BRIGHTNESS;
    }

    if (presetCommandIntervalActive && cmpTimeUs(currentTimeUs, nextPresetCommandAt) >= 0) {
        presetCommandIntervalActive = false;
    }

    if (nextPresetSetting < THERMAL_CAMERA_IMAGE_SETTING_COUNT &&
        !presetCommandIntervalActive &&
        thermalCameraSetImageSetting(lastPreset, (thermalCameraImageSetting_e)nextPresetSetting)) {
        nextPresetSetting++;
        presetCommandIntervalActive = true;
        nextPresetCommandAt = currentTimeUs + THERMAL_CAMERA_PRESET_COMMAND_INTERVAL_US;
    }
}

#endif
