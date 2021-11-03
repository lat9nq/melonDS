/*
    Copyright 2018-2019 Hydr8gon

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
    details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include <stdio.h>
#include <switch.h>

// Deal with conflicting typedefs
#define u64 u64_
#define s64 s64_

#include "PlatformConfig.h"

namespace Config {

int Mapping[15];

int ScreenRotation;
int ScreenGap;
int ScreenLayout;
int ScreenSizing;

int LimitFPS;

int DirectBoot;

int SavestateRelocSRAM;

int AudioVolume;
int MicInputType;

char LastROMFolder[512];

int SwitchOverclock;

ConfigEntry PlatformConfigFile[] = {
    {"Joy_A", 0, &Mapping[0], HidNpadButton_A, NULL, 0},
    {"Joy_B", 0, &Mapping[1], HidNpadButton_B, NULL, 0},
    {"Joy_Select", 0, &Mapping[2], HidNpadButton_Minus, NULL, 0},
    {"Joy_Start", 0, &Mapping[3], HidNpadButton_Plus, NULL, 0},
    {"Joy_Right", 0, &Mapping[4],
     HidNpadButton_Right | HidNpadButton_StickLRight, NULL, 0},
    {"Joy_Left", 0, &Mapping[5], HidNpadButton_Left | HidNpadButton_StickLLeft,
     NULL, 0},
    {"Joy_Up", 0, &Mapping[6], HidNpadButton_Up | HidNpadButton_StickLUp, NULL,
     0},
    {"Joy_Down", 0, &Mapping[7], HidNpadButton_Down | HidNpadButton_StickLDown,
     NULL, 0},
    {"Joy_R", 0, &Mapping[8], HidNpadButton_ZR, NULL, 0},
    {"Joy_L", 0, &Mapping[9], HidNpadButton_ZL, NULL, 0},
    {"Joy_X", 0, &Mapping[10], HidNpadButton_X, NULL, 0},
    {"Joy_Y", 0, &Mapping[11], HidNpadButton_Y, NULL, 0},
    {"Joy_Lid", 0, &Mapping[12], HidNpadButton_StickR, NULL, 0},
    {"Joy_Mic", 0, &Mapping[13], HidNpadButton_StickL, NULL, 0},
    {"Joy_Menu", 0, &Mapping[14], HidNpadButton_L | HidNpadButton_R, NULL, 0},

    {"ScreenRotation", 0, &ScreenRotation, 0, NULL, 0},
    {"ScreenGap", 0, &ScreenGap, 0, NULL, 0},
    {"ScreenLayout", 0, &ScreenLayout, 0, NULL, 0},
    {"ScreenSizing", 0, &ScreenSizing, 0, NULL, 0},

    {"LimitFPS", 0, &LimitFPS, 1, NULL, 0},

    {"DirectBoot", 0, &DirectBoot, 1, NULL, 0},

    {"SavStaRelocSRAM", 0, &SavestateRelocSRAM, 0, NULL, 0},

    {"AudioVolume", 0, &AudioVolume, 4, NULL, 0},
    {"MicInputType", 0, &MicInputType, 1, NULL, 0},

    {"LastROMFolder", 1, LastROMFolder, 0, (char *)"sdmc:/", 511},

    {"SwitchOverclock", 0, &SwitchOverclock, 0, NULL, 0},

    {"", -1, NULL, 0, NULL, 0}};

} // namespace Config
