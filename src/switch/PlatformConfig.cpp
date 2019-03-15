/*
    Copyright 2018-2019 Hydr8gon

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "PlatformConfig.h"

namespace Config
{

int JoyMapping[12];

int HKJoyMapping[HK_MAX];

int ScreenRotation;
int ScreenGap;
int ScreenLayout;
int ScreenSizing;
int ScreenFilter;

int LimitFPS;

int DirectBoot;

int SavestateRelocSRAM;

int AudioVolume;
int MicInputType;

char LastROMFolder[512];

int SwitchOverclock;

ConfigEntry PlatformConfigFile[] =
{
    {"Joy_A",      0, &JoyMapping[0],  -1, NULL, 0},
    {"Joy_B",      0, &JoyMapping[1],  -1, NULL, 0},
    {"Joy_Select", 0, &JoyMapping[2],  -1, NULL, 0},
    {"Joy_Start",  0, &JoyMapping[3],  -1, NULL, 0},
    {"Joy_Right",  0, &JoyMapping[4],  -1, NULL, 0},
    {"Joy_Left",   0, &JoyMapping[5],  -1, NULL, 0},
    {"Joy_Up",     0, &JoyMapping[6],  -1, NULL, 0},
    {"Joy_Down",   0, &JoyMapping[7],  -1, NULL, 0},
    {"Joy_R",      0, &JoyMapping[8],  -1, NULL, 0},
    {"Joy_L",      0, &JoyMapping[9],  -1, NULL, 0},
    {"Joy_X",      0, &JoyMapping[10], -1, NULL, 0},
    {"Joy_Y",      0, &JoyMapping[11], -1, NULL, 0},

    {"HKJoy_Lid",  0, &HKJoyMapping[HK_Lid],  -1, NULL, 0},
    {"HKJoy_Mic",  0, &HKJoyMapping[HK_Mic],  -1, NULL, 0},
    {"HKJoy_Menu", 0, &HKJoyMapping[HK_Menu], -1, NULL, 0},

    {"ScreenRotation", 0, &ScreenRotation, 0, NULL, 0},
    {"ScreenGap",      0, &ScreenGap,      0, NULL, 0},
    {"ScreenLayout",   0, &ScreenLayout,   0, NULL, 0},
    {"ScreenSizing",   0, &ScreenSizing,   0, NULL, 0},
    {"ScreenFilter",   0, &ScreenFilter,   1, NULL, 0},

    {"LimitFPS", 0, &LimitFPS, 1, NULL, 0},

    {"DirectBoot", 0, &DirectBoot, 1, NULL, 0},

    {"SavStaRelocSRAM", 0, &SavestateRelocSRAM, 0, NULL, 0},

    {"AudioVolume", 0, &AudioVolume, 256, NULL, 0},
    {"MicInputType", 0, &MicInputType, 1, NULL, 0},

    {"LastROMFolder", 1, LastROMFolder, 0, "", 511},

    {"SwitchOverclock", 0, &SwitchOverclock, 0, NULL, 0},

    {"", -1, NULL, 0, NULL, 0}
};

}
