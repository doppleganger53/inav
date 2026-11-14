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

#include <array>
#include <stdint.h>
#include <string.h>
#include <vector>

#include "gtest/gtest.h"

extern "C" {
    #include "drivers/serial.h"
    #include "io/thermal_camera.h"
}

static int32_t testGvars[4];
static uint32_t testTxBytesFree;
static std::vector<std::array<uint8_t, THERMAL_CAMERA_PACKET_SIZE>> testPackets;

extern "C" int32_t gvGet(uint8_t index)
{
    return testGvars[index];
}

extern "C" uint32_t serialTxBytesFree(const serialPort_t *instance)
{
    EXPECT_NE(nullptr, instance);
    return testTxBytesFree;
}

extern "C" void serialWriteBuf(serialPort_t *instance, const uint8_t *data, int count)
{
    EXPECT_NE(nullptr, instance);
    ASSERT_EQ(THERMAL_CAMERA_PACKET_SIZE, count);

    std::array<uint8_t, THERMAL_CAMERA_PACKET_SIZE> packet;
    memcpy(packet.data(), data, packet.size());
    testPackets.push_back(packet);
}

class ThermalCameraRuntimeTest : public testing::Test {
protected:
    void SetUp() override
    {
        memset(testGvars, 0, sizeof(testGvars));
        testTxBytesFree = THERMAL_CAMERA_PACKET_SIZE;
        testPackets.clear();
        thermalCameraTestInit(false);
    }

    static void expectPacket(size_t index, const std::array<uint8_t, THERMAL_CAMERA_PACKET_SIZE>& expected)
    {
        ASSERT_LT(index, testPackets.size());
        EXPECT_EQ(expected, testPackets[index]);
    }
};

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
        { 0x78, 0x02, 0x32, { 0xF0, 0x05, 0x36, 0x78, 0x02, 0x00, 0x32, 0xE2, 0xFF } },
        { 0x78, 0x03, 0x32, { 0xF0, 0x05, 0x36, 0x78, 0x03, 0x00, 0x32, 0xE3, 0xFF } },
        { 0x78, 0x10, 0x32, { 0xF0, 0x05, 0x36, 0x78, 0x10, 0x00, 0x32, 0xF0, 0xFF } },
        { 0x78, 0x15, 0x32, { 0xF0, 0x05, 0x36, 0x78, 0x15, 0x00, 0x32, 0xF5, 0xFF } },
        { 0x78, 0x16, 0x32, { 0xF0, 0x05, 0x36, 0x78, 0x16, 0x00, 0x32, 0xF6, 0xFF } },
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
    EXPECT_EQ(0x00, thermalCameraPaletteCode(0, false));
    EXPECT_EQ(0x01, thermalCameraPaletteCode(1, false));
    EXPECT_EQ(0x05, thermalCameraPaletteCode(2, false));
    EXPECT_EQ(0x02, thermalCameraPaletteCode(3, false));
    EXPECT_EQ(0x00, thermalCameraPaletteCode(-1, false));
    EXPECT_EQ(0x02, thermalCameraPaletteCode(4, false));

    for (int32_t palette = 0; palette <= 14; palette++) {
        EXPECT_EQ(palette, thermalCameraPaletteCode(palette, true));
    }
    EXPECT_EQ(0x00, thermalCameraPaletteCode(-1, true));
    EXPECT_EQ(0x0E, thermalCameraPaletteCode(15, true));
}

TEST(ThermalCameraTest, MapsAndClampsImagePresets)
{
    static const uint8_t expected[][THERMAL_CAMERA_IMAGE_SETTING_COUNT] = {
        { 50, 50, 50, 50, 50 },
        { 50, 55, 70, 30, 30 },
        { 50, 50, 40, 70, 70 },
        { 50, 70, 60, 45, 45 },
    };

    EXPECT_EQ(0, thermalCameraPresetIndex(-1));
    EXPECT_EQ(0, thermalCameraPresetIndex(0));
    EXPECT_EQ(3, thermalCameraPresetIndex(3));
    EXPECT_EQ(3, thermalCameraPresetIndex(4));

    for (int32_t preset = 0; preset < 4; preset++) {
        for (int setting = 0; setting < THERMAL_CAMERA_IMAGE_SETTING_COUNT; setting++) {
            EXPECT_EQ(expected[preset][setting],
                thermalCameraPresetValue(preset, (thermalCameraImageSetting_e)setting));
        }
    }
    EXPECT_EQ(50, thermalCameraPresetValue(-1, THERMAL_CAMERA_IMAGE_BRIGHTNESS));
    EXPECT_EQ(45, thermalCameraPresetValue(4, THERMAL_CAMERA_IMAGE_DYNAMIC_DENOISE));
}

TEST_F(ThermalCameraRuntimeTest, SendsNothingAtStartup)
{
    testGvars[0] = 5;
    testGvars[1] = 7;
    testGvars[2] = 3;
    testGvars[3] = 9;
    thermalCameraTestInit(true);

    thermalCameraUpdate(1000000);
    thermalCameraUpdate(2000000);
    EXPECT_TRUE(testPackets.empty());
}

TEST_F(ThermalCameraRuntimeTest, PreservesPaletteAndFfcChangeOnlyBehavior)
{
    testGvars[0] = 2;
    testGvars[1] = 1;
    thermalCameraUpdate(0);

    ASSERT_EQ(2, testPackets.size());
    expectPacket(0, { 0xF0, 0x05, 0x36, 0x78, 0x20, 0x00, 0x05, 0xD3, 0xFF });
    expectPacket(1, { 0xF0, 0x05, 0x36, 0x7C, 0x02, 0x00, 0x00, 0xB4, 0xFF });

    thermalCameraUpdate(100000);
    EXPECT_EQ(2, testPackets.size());
}

TEST_F(ThermalCameraRuntimeTest, SendsPresetInOrderAtTwoHundredFiftyMillisecondIntervals)
{
    testGvars[2] = 1;
    thermalCameraUpdate(1000000);
    thermalCameraUpdate(1249999);
    thermalCameraUpdate(1250000);
    thermalCameraUpdate(1500000);
    thermalCameraUpdate(1750000);
    thermalCameraUpdate(2000000);

    ASSERT_EQ(5, testPackets.size());
    expectPacket(0, { 0xF0, 0x05, 0x36, 0x78, 0x02, 0x00, 0x32, 0xE2, 0xFF });
    expectPacket(1, { 0xF0, 0x05, 0x36, 0x78, 0x03, 0x00, 0x37, 0xE8, 0xFF });
    expectPacket(2, { 0xF0, 0x05, 0x36, 0x78, 0x10, 0x00, 0x46, 0x04, 0xFF });
    expectPacket(3, { 0xF0, 0x05, 0x36, 0x78, 0x15, 0x00, 0x1E, 0xE1, 0xFF });
    expectPacket(4, { 0xF0, 0x05, 0x36, 0x78, 0x16, 0x00, 0x1E, 0xE2, 0xFF });
}

TEST_F(ThermalCameraRuntimeTest, ReplacesUnfinishedPresetWithoutBreakingPacing)
{
    testGvars[2] = 1;
    thermalCameraUpdate(1000000);

    testGvars[2] = 3;
    thermalCameraUpdate(1100000);
    ASSERT_EQ(1, testPackets.size());

    thermalCameraUpdate(1250000);
    thermalCameraUpdate(1500000);

    ASSERT_EQ(3, testPackets.size());
    expectPacket(1, { 0xF0, 0x05, 0x36, 0x78, 0x02, 0x00, 0x32, 0xE2, 0xFF });
    expectPacket(2, { 0xF0, 0x05, 0x36, 0x78, 0x03, 0x00, 0x46, 0xF7, 0xFF });
}

TEST_F(ThermalCameraRuntimeTest, RetriesPresetAndBackgroundCorrectionWhenTxCapacityReturns)
{
    testTxBytesFree = THERMAL_CAMERA_PACKET_SIZE - 1;
    testGvars[2] = 2;
    testGvars[3] = 1;
    thermalCameraUpdate(1000000);
    EXPECT_TRUE(testPackets.empty());

    testTxBytesFree = THERMAL_CAMERA_PACKET_SIZE;
    thermalCameraUpdate(1050000);
    ASSERT_EQ(2, testPackets.size());
    expectPacket(0, { 0xF0, 0x05, 0x36, 0x7C, 0x03, 0x00, 0x00, 0xB5, 0xFF });
    expectPacket(1, { 0xF0, 0x05, 0x36, 0x78, 0x02, 0x00, 0x32, 0xE2, 0xFF });

    thermalCameraUpdate(1100000);
    EXPECT_EQ(2, testPackets.size());

    testGvars[3] = 2;
    thermalCameraUpdate(1150000);
    ASSERT_EQ(3, testPackets.size());
    expectPacket(2, { 0xF0, 0x05, 0x36, 0x7C, 0x03, 0x00, 0x00, 0xB5, 0xFF });
}
