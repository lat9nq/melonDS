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

#ifndef PLATFORMCONFIG_H
#define PLATFORMCONFIG_H

#include "../Config.h"

enum
{
    HK_Lid = 0,
    HK_Mic,
    HK_Menu,

    HK_MAX
};

namespace Config
{

extern int Mapping[12];

extern int HKMapping[HK_MAX];

extern int ScreenRotation;
extern int ScreenGap;
extern int ScreenLayout;
extern int ScreenSizing;
extern int ScreenFilter;

extern int LimitFPS;

extern int DirectBoot;

extern int SavestateRelocSRAM;

extern int AudioVolume;
extern int MicInputType;

extern char LastROMFolder[512];

extern int SwitchOverclock;

}

#endif // PLATFORMCONFIG_H
