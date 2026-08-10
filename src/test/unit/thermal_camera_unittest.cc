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

#include <stdint.h>
#include <string.h>

#include "gtest/gtest.h"

extern "C" {
    #include "io/thermal_camera.h"
}

typedef struct thermalCameraPacketTest_s {
    uint8_t commandClass;
    uint8_t subcommand;
    uint8_t value;
    uint8_t expected[THERMAL_CAMERA_PACKET_SIZE];
} thermalCameraPacketTest_t;

TEST(ThermalCameraTest, BuildsConfirmedPackets)
{
    const thermalCameraPacketTest_t tests[] = {
        { 0x78, 0x20, 0x00, { 0xF0, 0x05, 0x36, 0x78, 0x20, 0x00, 0x00, 0xCE, 0xFF } },
        { 0x78, 0x20, 0x01, { 0xF0, 0x05, 0x36, 0x78, 0x20, 0x00, 0x01, 0xCF, 0xFF } },
        { 0x78, 0x20, 0x02, { 0xF0, 0x05, 0x36, 0x78, 0x20, 0x00, 0x02, 0xD0, 0xFF } },
        { 0x78, 0x20, 0x05, { 0xF0, 0x05, 0x36, 0x78, 0x20, 0x00, 0x05, 0xD3, 0xFF } },
        { 0x7C, 0x02, 0x00, { 0xF0, 0x05, 0x36, 0x7C, 0x02, 0x00, 0x00, 0xB4, 0xFF } },
        { 0x7C, 0x03, 0x00, { 0xF0, 0x05, 0x36, 0x7C, 0x03, 0x00, 0x00, 0xB5, 0xFF } },
        { 0x74, 0x10, 0x00, { 0xF0, 0x05, 0x36, 0x74, 0x10, 0x00, 0x00, 0xBA, 0xFF } },
    };

    for (const thermalCameraPacketTest_t &test : tests) {
        uint8_t packet[THERMAL_CAMERA_PACKET_SIZE];
        thermalCameraBuildPacket(test.commandClass, test.subcommand, 0, test.value, packet);
        EXPECT_EQ(0, memcmp(test.expected, packet, sizeof(packet)));
    }
}

TEST(ThermalCameraTest, MapsAndClampsLogicalPalettes)
{
    EXPECT_EQ(0x00, thermalCameraPaletteCode(0));
    EXPECT_EQ(0x01, thermalCameraPaletteCode(1));
    EXPECT_EQ(0x05, thermalCameraPaletteCode(2));
    EXPECT_EQ(0x02, thermalCameraPaletteCode(3));
    EXPECT_EQ(0x00, thermalCameraPaletteCode(-1));
    EXPECT_EQ(0x02, thermalCameraPaletteCode(4));
}
