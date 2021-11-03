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
    melonDS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
    details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include <chrono>
#include <cstring>
#include <limits>
#include <unistd.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <glad/glad.h>

#include "ui.h"

// Deal with conflicting typedefs
#define u64 u64_
#define s64 s64_

#include "../GPU.h"
#include "../NDS.h"
#include "../OpenGLSupport.h"
#include "../Platform.h"
#include "../SPU.h"
#include "../Savestate.h"
#include "../libui_sdl/main_shaders.h"
#include "PlatformConfig.h"

typedef struct {
    vector<string> names;
    int* value;
} SettingValue;

string romPath, sramPath, statePath, sramStatePath;

u32 displayBuffer[256 * 384];
float topX, topY, topWidth, topHeight, botX, botY, botWidth, botHeight;

Thread audio, mic;
ClkrstSession cpuSession;
AppletHookCookie cookie;

AudioOutBuffer* audioReleasedBuffer;
AudioInBuffer micBuffer, *micReleasedBuffer;
u32 count;

u8 hotkeyMask;
bool lidClosed;

GLuint GL_ScreenShader[3];
GLuint GL_ScreenShaderAccel[3];
struct {
    float uScreenSize[2];
    u32 u3DScale;
    u32 uFilterMode;
} GL_ShaderConfig;
GLuint GL_ShaderConfigUBO;
GLuint GL_ScreenVertexArrayID, GL_ScreenVertexBufferID;
float GL_ScreenVertices[2 * 3 * 2 * 4];
GLuint GL_ScreenTexture;
bool GL_ScreenSizeDirty;

const int clockSpeeds[] = {1020000000, 1224000000, 1581000000, 1785000000};

const vector<string> controlNames = {
    "A Button",   "B Button", "Select Button",  "Start Button", "D-Pad Right",
    "D-Pad Left", "D-Pad Up", "D-Pad Down",     "R Button",     "L Button",
    "X Button",   "Y Button", "Close/Open Lid", "Microphone",   "Pause Menu"};

const vector<string> controlValues = {
    "A Button",         "B Button",          "X Button",          "Y Button",
    "Left Stick Click", "Right Stick Click", "L Button",          "R Button",
    "ZL Button",        "ZR Button",         "Plus Button",       "Minus Button",
    "D-Pad Left",       "D-Pad Up",          "D-Pad Right",       "D-Pad Down",
    "Left Stick Left",  "Left Stick Up",     "Left Stick Right",  "Left Stick Down",
    "Right Stick Left", "Right Stick Up",    "Right Stick Right", "Right Stick Down"};

const vector<string> settingNames = {"Boot Game Directly", "Threaded Software Renderer",
                                     "OpenGL Renderer",    "OpenGL Resolution",
                                     "Frameskip",          "Audio Volume",
                                     "Microphone Input",   "Separate Savefiles from Savestates",
                                     "Screen Rotation",    "Mid-Screen Gap",
                                     "Screen Layout",      "Screen Sizing",
                                     "Limit Framerate",    "Switch Overclock"};

const vector<SettingValue> settingValues = {
    {{"Off", "On"}, &Config::DirectBoot},
    {{"Off", "On"}, &Config::Threaded3D},
    {{"Off", "On"}, &Config::_3DRenderer},
    {{"", "1x (256x192)", "2x (512x384)", "3x (768x576)", "4x (1024x768)"},
     &Config::GL_ScaleFactor},
    {{"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"}, &Config::Frameskip},
    {{"0%", "25%", "50%", "75%", "100%"}, &Config::AudioVolume},
    {{"None", "Microphone", "White Noise"}, &Config::MicInputType},
    {{"Off", "On"}, &Config::SavestateRelocSRAM},
    {{"0", "90", "180", "270"}, &Config::ScreenRotation},
    {{"0 Pixels", "1 Pixel", "8 Pixels", "64 Pixels", "90 Pixels", "128 Pixels"},
     &Config::ScreenGap},
    {{"Natural", "Vertical", "Horizontal"}, &Config::ScreenLayout},
    {{"Even", "Emphasize Top", "Emphasize Bottom"}, &Config::ScreenSizing},
    {{"Off", "On"}, &Config::LimitFPS},
    {{"1020 MHz", "1224 MHz", "1581 MHz", "1785 MHz"}, &Config::SwitchOverclock}};

const vector<string> pauseNames = {"Resume", "Save State", "Load State", "Settings",
                                   "File Browser"};

u32* romIcon(string filename) {
    FILE* rom = fopen(filename.c_str(), "rb");
    if (!rom)
        return NULL;

    u32 offset;
    fseek(rom, 0x68, SEEK_SET);
    fread(&offset, sizeof(u32), 1, rom);

    u8 data[512];
    fseek(rom, 0x20 + offset, SEEK_SET);
    fread(data, sizeof(u8), 512, rom);

    u16 palette[16];
    fseek(rom, 0x220 + offset, SEEK_SET);
    fread(palette, sizeof(u16), 16, rom);

    fclose(rom);

    // Get the 4-bit palette indexes
    u8 indexes[1024];
    for (int i = 0; i < 512; i++) {
        indexes[i * 2] = data[i] & 0x0F;
        indexes[i * 2 + 1] = data[i] >> 4;
    }

    // Get each pixel's 5-bit palette color and convert it to 8-bit
    u32 tiles[32 * 32];
    for (int i = 0; i < 1024; i++) {
        u8 r = ((palette[indexes[i]] >> 0) & 0x1F) * 255 / 31;
        u8 g = ((palette[indexes[i]] >> 5) & 0x1F) * 255 / 31;
        u8 b = ((palette[indexes[i]] >> 10) & 0x1F) * 255 / 31;
        tiles[i] = rgbaToU32(r, g, b, 255);
    }

    // Rearrange the pixels from 8x8 tiles to a 32x32 texture
    u32* tex = new u32[32 * 32];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 8; j++)
            for (int k = 0; k < 4; k++)
                memcpy(&tex[256 * i + 32 * j + 8 * k], &tiles[256 * i + 8 * j + 64 * k],
                       8 * sizeof(u32));

    return tex;
}

void swapValues(float* val1, float* val2) {
    float temp = *val1;
    *val1 = *val2;
    *val2 = temp;
}

void setScreenLayout() {
    int gapSizes[] = {0, 1, 8, 64, 90, 128};
    float gap = gapSizes[Config::ScreenGap];

    if (Config::ScreenLayout == 0) // Natural, choose based on rotation
        Config::ScreenLayout = (Config::ScreenRotation % 2 == 0) ? 1 : 2;

    if (Config::ScreenLayout == 1) // Vertical
    {
        if (Config::ScreenSizing == 0) // Even
        {
            topHeight = botHeight = 360 - gap / 2;
            if (Config::ScreenRotation % 2 == 0)
                topWidth = botWidth = topHeight * 4 / 3;
            else
                topWidth = botWidth = topHeight * 3 / 4;
        } else if (Config::ScreenSizing == 1) // Emphasize top
        {
            if (Config::ScreenRotation % 2 == 0) // 0, 180
            {
                botWidth = 256;
                botHeight = 192;
                topHeight = 720 - botHeight - gap;
                topWidth = topHeight * 4 / 3;
            } else // 90, 270
            {
                botWidth = 192;
                botHeight = 256;
                topHeight = 720 - botHeight - gap;
                topWidth = topHeight * 3 / 4;
            }
        } else // Emphasize bottom
        {
            if (Config::ScreenRotation % 2 == 0) // 0, 180
            {
                topWidth = 256;
                topHeight = 192;
                botHeight = 720 - topHeight - gap;
                botWidth = botHeight * 4 / 3;
            } else // 90, 270
            {
                botWidth = 192;
                botHeight = 256;
                topHeight = 720 - botHeight - gap;
                topWidth = topHeight * 3 / 4;
            }
        }

        topX = 640 - topWidth / 2;
        botX = 640 - botWidth / 2;
        topY = 0;
        botY = 720 - botHeight;
    } else // Horizontal
    {
        if (Config::ScreenRotation % 2 == 0) // 0, 180
        {
            topWidth = botWidth = 640 - gap / 2;
            topHeight = botHeight = topWidth * 3 / 4;
            topX = 0;
            botX = 1280 - topWidth;
        } else // 90, 270
        {
            topHeight = botHeight = 720;
            topWidth = botWidth = topHeight * 3 / 4;
            topX = 640 - topWidth - gap / 2;
            botX = 640 + gap / 2;
        }

        topY = botY = 360 - topHeight / 2;

        if (Config::ScreenSizing == 1) // Emphasize top
        {
            if (Config::ScreenRotation % 2 == 0) // 0, 180
            {
                botWidth = 256;
                botHeight = 192;
                topWidth = 1280 - botWidth - gap;
                if (topWidth > 960)
                    topWidth = 960;
                topHeight = topWidth * 3 / 4;
                topX = 640 - (botWidth + topWidth + gap) / 2;
                botX = topX + topWidth + gap;
                topY = 360 - topHeight / 2;
                botY = topY + topHeight - botHeight;
            } else // 90, 270
            {
                botWidth = 192;
                botHeight = 256;
                topX += (topWidth - botWidth) / 2;
                botX += (topWidth - botWidth) / 2;
                botY = 720 - botHeight;
            }
        } else if (Config::ScreenSizing == 2) // Emphasize bottom
        {
            if (Config::ScreenRotation % 2 == 0) // 0, 180
            {
                topWidth = 256;
                topHeight = 192;
                botWidth = 1280 - topWidth - gap;
                if (botWidth > 960)
                    botWidth = 960;
                botHeight = botWidth * 3 / 4;
                topX = 640 - (botWidth + topWidth + gap) / 2;
                botX = topX + topWidth + gap;
                botY = 360 - botHeight / 2;
                topY = botY + botHeight - topHeight;
            } else // 90, 270
            {
                topWidth = 192;
                topHeight = 256;
                topX += (botWidth - topWidth) / 2;
                botX -= (botWidth - topWidth) / 2;
                topY = 720 - topHeight;
            }
        }
    }

    // Swap the top and bottom screens for 90 and 180 degrees
    if (Config::ScreenRotation == 1 || Config::ScreenRotation == 2) {
        swapValues(&topX, &botX);
        swapValues(&topY, &botY);
        swapValues(&topWidth, &botWidth);
        swapValues(&topHeight, &botHeight);
    }

    GL_ScreenSizeDirty = true;
}

void onAppletHook(AppletHookType hook, void* param) {
    if (hook == AppletHookType_OnOperationMode || hook == AppletHookType_OnPerformanceMode) {
        if (R_FAILED(pcvSetClockRate(PcvModule_CpuBus, clockSpeeds[Config::SwitchOverclock])))
            clkrstSetClockRate(&cpuSession, clockSpeeds[Config::SwitchOverclock]);
    }
}

void fillAudioBuffer() {
    // 1024 samples is equal to approximately 700 at the original rate
    s16 buf_in[700 * 2];
    s16* buf_out = (s16*)audioReleasedBuffer->buffer;

    int num_in = SPU::ReadOutput(buf_in, 700);

    int margin = 6;
    if (num_in < 700 - margin) {
        int last = num_in - 1;
        if (last < 0)
            last = 0;

        for (int i = num_in; i < 700 - margin; i++)
            ((u32*)buf_in)[i] = ((u32*)buf_in)[last];

        num_in = 700 - margin;
    }

    float res_incr = (float)num_in / 1024;
    float res_timer = 0;
    int res_pos = 0;

    for (int i = 0; i < 1024; i++) {
        buf_out[i * 2] = (buf_in[res_pos * 2] * Config::AudioVolume * 64) >> 8;
        buf_out[i * 2 + 1] = (buf_in[res_pos * 2 + 1] * Config::AudioVolume * 64) >> 8;

        res_timer += res_incr;
        while (res_timer >= 1) {
            res_timer--;
            res_pos++;
        }
    }
}

void audioOutput(void* args) {
    while (!(hotkeyMask & BIT(2))) {
        audoutWaitPlayFinish(&audioReleasedBuffer, &count, std::numeric_limits<u_int64_t>::max());
        fillAudioBuffer();
        audoutAppendAudioOutBuffer(audioReleasedBuffer);
    }
}

void micInput(void* args) {
    while (!(hotkeyMask & BIT(2))) {
        if (Config::MicInputType == 0 || !(hotkeyMask & BIT(1))) {
            NDS::MicInputFrame(NULL, 0);
        } else if (Config::MicInputType == 1) {
            audinCaptureBuffer(&micBuffer, &micReleasedBuffer);
            NDS::MicInputFrame((s16*)micBuffer.buffer, 1440);
        } else {
            s16 input[1440];
            for (int i = 0; i < 1440; i++)
                input[i] = rand() & 0xFFFF;
            NDS::MicInputFrame(input, 1440);
        }
    }
}

void startCore(bool reset) {
    setScreenLayout();

    appletLockExit();
    appletHook(&cookie, onAppletHook, NULL);

    if (reset) {
        sramPath = romPath.substr(0, romPath.rfind(".")) + ".sav";
        statePath = romPath.substr(0, romPath.rfind(".")) + ".mln";
        sramStatePath = statePath + ".sav";

        NDS::Init();
        NDS::LoadROM(romPath.c_str(), sramPath.c_str(), Config::DirectBoot);
    }

    if (Config::AudioVolume > 0) {
        audoutInitialize();
        audoutStartAudioOut();
        setupAudioBuffer();
        threadCreate(&audio, audioOutput, nullptr, nullptr, 0x8000, 0x30, 1);
        threadStart(&audio);
    }
    if (Config::MicInputType == 1) {
        audinInitialize();
        audinStartAudioIn();
        threadCreate(&mic, micInput, nullptr, nullptr, 0x8000, 0x30, 0);
        threadStart(&mic);
    }
    if (Config::SwitchOverclock > 0) {
        pcvInitialize();
        if (R_FAILED(pcvSetClockRate(PcvModule_CpuBus, clockSpeeds[Config::SwitchOverclock]))) {
            clkrstInitialize();
            clkrstOpenSession(&cpuSession, PcvModuleId_CpuBus, 0);
            clkrstSetClockRate(&cpuSession, clockSpeeds[Config::SwitchOverclock]);
        }
    }

    GPU3D::DeInitRenderer();
    if (Config::_3DRenderer)
        GPU3D::InitRenderer(true);
    else
        GPU3D::InitRenderer(false);
}

void pauseCore() {
    threadWaitForExit(&audio);
    threadWaitForExit(&mic);
    if (R_FAILED(pcvSetClockRate(PcvModule_CpuBus, clockSpeeds[0]))) {
        clkrstSetClockRate(&cpuSession, clockSpeeds[0]);
        clkrstExit();
    }
    pcvExit();
    audinStopAudioIn();
    audinExit();
    audoutStopAudioOut();
    audoutExit();
    appletUnhook(&cookie);
    appletUnlockExit();
}

void controlsMenu() {
    int selection = 0;

    while (true) {
        vector<string> controlSubitems;
        for (unsigned int i = 0; i < controlNames.size(); i++) {
            if (Config::Mapping[i] == 0) {
                controlSubitems.push_back("None");
            } else {
                string subitem;
                int count = 0;
                for (unsigned int j = 0; j < controlValues.size(); j++) {
                    if (Config::Mapping[i] & BIT(j)) {
                        count++;
                        if (count < 5) {
                            subitem += controlValues[j] + ", ";
                        } else {
                            subitem += "...";
                            break;
                        }
                    }
                }
                controlSubitems.push_back(
                    subitem.substr(0, subitem.size() - ((count == 5) ? 0 : 2)));
            }
        }

        u32 pressed =
            menuScreen("Controls", "", "Clear", {}, controlNames, controlSubitems, &selection);

        if (pressed & HidNpadButton_A) {
            pressed = 0;
            while (pressed == 0 || pressed > HidNpadButton_StickRDown)
                pressed = messageScreen(
                    "Controls", {"Press a button to add a mapping to: " + controlNames[selection]},
                    false);
            Config::Mapping[selection] |= pressed;
        } else if (pressed & HidNpadButton_B) {
            return;
        } else if ((pressed & HidNpadButton_X) && !(pressed & HidGestureType_Touch)) {
            Config::Mapping[selection] = 0;
        }
    }
}

void settingsMenu() {
    int selection = 0;

    while (true) {
        vector<string> settingSubitems;
        for (unsigned int i = 0; i < settingNames.size(); i++)
            settingSubitems.push_back(settingValues[i].names[*settingValues[i].value]);

        u32 pressed =
            menuScreen("Settings", "", "Controls", {}, settingNames, settingSubitems, &selection);

        if (pressed & HidNpadButton_A) {
            (*settingValues[selection].value)++;
            if (*settingValues[selection].value >= (int)settingValues[selection].names.size())
                *settingValues[selection].value = (selection == 3 ? 1 : 0);
        } else if (pressed & HidNpadButton_B) {
            Config::Save();
            return;
        } else if (pressed & HidNpadButton_X) {
            controlsMenu();
        }
    }
}

bool fileBrowser() {
    int selection = 0;
    romPath = Config::LastROMFolder;

    while (true) {
        vector<string> files = dirContents(romPath, ".nds");
        vector<Icon> icons;

        for (unsigned int i = 0; i < files.size(); i++) {
            if (files[i].find(".nds", (files[i].length() - 4)) != string::npos)
                icons.push_back({romIcon(romPath + "/" + files[i]), 32});
            else
                icons.push_back({folderIcon, 64});
        }

        u32 pressed = menuScreen("melonDS", "Exit", "Settings", icons, files, {}, &selection);

        if (pressed & HidNpadButton_A && files.size() > 0) {
            romPath += "/" + files[selection];
            selection = 0;

            if (romPath.find(".nds", romPath.length() - 4) != string::npos)
                break;
        } else if (pressed & HidNpadButton_B && romPath != "sdmc:/") {
            romPath = romPath.substr(0, romPath.rfind("/"));
            selection = 0;
        } else if (pressed & HidNpadButton_X) {
            settingsMenu();
        } else if (pressed & HidNpadButton_Plus) {
            return false;
        }
    }

    string folder = romPath.substr(0, romPath.rfind("/"));
    folder.append(1, '\0');
    strncpy(Config::LastROMFolder, folder.c_str(), folder.length());

    return true;
}

bool pauseMenu() {
    pauseCore();

    int selection = 0;

    while (true) {
        u32 pressed = menuScreen("melonDS", "", "", {}, pauseNames, {}, &selection);

        if (pressed & HidNpadButton_A) {
            if (selection == 0) // Resume
            {
                break;
            } else if (selection == 1 || selection == 2) // Save/load state
            {
                Savestate* state =
                    new Savestate(const_cast<char*>(statePath.c_str()), selection == 1);
                if (!state->Error) {
                    NDS::DoSavestate(state);
                    if (Config::SavestateRelocSRAM)
                        NDS::RelocateSave(const_cast<char*>(sramStatePath.c_str()), selection == 1);
                }
                delete state;
                break;
            } else if (selection == 3) // Settings
            {
                settingsMenu();
            } else // File browser
            {
                NDS::DeInit();
                if (fileBrowser()) {
                    hotkeyMask &= ~BIT(2);
                    startCore(true);
                    return true;
                }
                return false;
            }
        } else if (pressed & HidNpadButton_B) {
            break;
        }
    }

    hotkeyMask &= ~BIT(2);
    startCore(false);
    return true;
}

bool GLScreen_InitShader(GLuint* shader, const char* fs) {
    if (!GPU3D::GLRenderer::OpenGL_BuildShaderProgram(kScreenVS, fs, shader, "ScreenShader"))
        return false;

    glBindAttribLocation(shader[2], 0, "vPosition");
    glBindAttribLocation(shader[2], 1, "vTexcoord");
    glBindFragDataLocation(shader[2], 0, "oColor");

    if (!GPU3D::GLRenderer::OpenGL_LinkShaderProgram(shader))
        return false;

    GLuint uni_id;

    uni_id = glGetUniformBlockIndex(shader[2], "uConfig");
    glUniformBlockBinding(shader[2], uni_id, 16);

    glUseProgram(shader[2]);
    uni_id = glGetUniformLocation(shader[2], "ScreenTex");
    glUniform1i(uni_id, 0);
    uni_id = glGetUniformLocation(shader[2], "_3DTex");
    glUniform1i(uni_id, 1);

    return true;
}

bool GLScreen_Init() {
    if (!GPU3D::GLRenderer::OpenGL_Init())
        return false;

    if (!GLScreen_InitShader(GL_ScreenShader, kScreenFS))
        return false;
    if (!GLScreen_InitShader(GL_ScreenShaderAccel, kScreenFS_Accel))
        return false;

    memset(&GL_ShaderConfig, 0, sizeof(GL_ShaderConfig));

    glGenBuffers(1, &GL_ShaderConfigUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, GL_ShaderConfigUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(GL_ShaderConfig), &GL_ShaderConfig, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 16, GL_ShaderConfigUBO);

    glGenBuffers(1, &GL_ScreenVertexBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, GL_ScreenVertexBufferID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GL_ScreenVertices), NULL, GL_STATIC_DRAW);

    glGenVertexArrays(1, &GL_ScreenVertexArrayID);
    glBindVertexArray(GL_ScreenVertexArrayID);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * 4, (void*)(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * 4, (void*)(2 * 4));

    glGenTextures(1, &GL_ScreenTexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, GL_ScreenTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 256 * 3 + 1, 192 * 2, 0, GL_RGBA_INTEGER,
                 GL_UNSIGNED_BYTE, NULL);

    GL_ScreenSizeDirty = true;

    return true;
}

void GLScreen_DrawScreen() {
    if (GL_ScreenSizeDirty) {
        GL_ScreenSizeDirty = false;

        GL_ShaderConfig.uScreenSize[0] = 1280;
        GL_ShaderConfig.uScreenSize[1] = 720;
        GL_ShaderConfig.u3DScale = Config::GL_ScaleFactor;

        glBindBuffer(GL_UNIFORM_BUFFER, GL_ShaderConfigUBO);
        void* unibuf = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
        if (unibuf)
            memcpy(unibuf, &GL_ShaderConfig, sizeof(GL_ShaderConfig));
        glUnmapBuffer(GL_UNIFORM_BUFFER);

        float scwidth, scheight;

        float x0, y0, x1, y1;
        float s0, s1, s2, s3;
        float t0, t1, t2, t3;

#define SETVERTEX(i, x, y, s, t)                                                                   \
    GL_ScreenVertices[4 * (i) + 0] = x;                                                            \
    GL_ScreenVertices[4 * (i) + 1] = y;                                                            \
    GL_ScreenVertices[4 * (i) + 2] = s;                                                            \
    GL_ScreenVertices[4 * (i) + 3] = t;

        x0 = topX;
        y0 = topY;
        x1 = topX + topWidth;
        y1 = topY + topHeight;

        scwidth = 256;
        scheight = 192;

        switch (Config::ScreenRotation) {
        case 0:
            s0 = 0;
            t0 = 0;
            s1 = scwidth;
            t1 = 0;
            s2 = 0;
            t2 = scheight;
            s3 = scwidth;
            t3 = scheight;
            break;

        case 1:
            s0 = 0;
            t0 = scheight;
            s1 = 0;
            t1 = 0;
            s2 = scwidth;
            t2 = scheight;
            s3 = scwidth;
            t3 = 0;
            break;

        case 2:
            s0 = scwidth;
            t0 = scheight;
            s1 = 0;
            t1 = scheight;
            s2 = scwidth;
            t2 = 0;
            s3 = 0;
            t3 = 0;
            break;

        default:
            s0 = scwidth;
            t0 = 0;
            s1 = scwidth;
            t1 = scheight;
            s2 = 0;
            t2 = 0;
            s3 = 0;
            t3 = scheight;
            break;
        }

        SETVERTEX(0, x0, y0, s0, t0);
        SETVERTEX(1, x1, y1, s3, t3);
        SETVERTEX(2, x1, y0, s1, t1);
        SETVERTEX(3, x0, y0, s0, t0);
        SETVERTEX(4, x0, y1, s2, t2);
        SETVERTEX(5, x1, y1, s3, t3);

        x0 = botX;
        y0 = botY;
        x1 = botX + botWidth;
        y1 = botY + botHeight;

        scwidth = 256;
        scheight = 192;

        switch (Config::ScreenRotation) {
        case 0:
            s0 = 0;
            t0 = 192;
            s1 = scwidth;
            t1 = 192;
            s2 = 0;
            t2 = 192 + scheight;
            s3 = scwidth;
            t3 = 192 + scheight;
            break;

        case 1:
            s0 = 0;
            t0 = 192 + scheight;
            s1 = 0;
            t1 = 192;
            s2 = scwidth;
            t2 = 192 + scheight;
            s3 = scwidth;
            t3 = 192;
            break;

        case 2:
            s0 = scwidth;
            t0 = 192 + scheight;
            s1 = 0;
            t1 = 192 + scheight;
            s2 = scwidth;
            t2 = 192;
            s3 = 0;
            t3 = 192;
            break;

        default:
            s0 = scwidth;
            t0 = 192;
            s1 = scwidth;
            t1 = 192 + scheight;
            s2 = 0;
            t2 = 192;
            s3 = 0;
            t3 = 192 + scheight;
            break;
        }

        SETVERTEX(6, x0, y0, s0, t0);
        SETVERTEX(7, x1, y1, s3, t3);
        SETVERTEX(8, x1, y0, s1, t1);
        SETVERTEX(9, x0, y0, s0, t0);
        SETVERTEX(10, x0, y1, s2, t2);
        SETVERTEX(11, x1, y1, s3, t3);

#undef SETVERTEX

        glBindBuffer(GL_ARRAY_BUFFER, GL_ScreenVertexBufferID);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GL_ScreenVertices), GL_ScreenVertices);

        GPU3D::UpdateRendererConfig();
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glViewport(0, 0, 1280, 720);

    if (GPU3D::Renderer == 0)
        GPU3D::GLRenderer::OpenGL_UseShaderProgram(GL_ScreenShader);
    else
        GPU3D::GLRenderer::OpenGL_UseShaderProgram(GL_ScreenShaderAccel);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    {
        int frontbuf = GPU::FrontBuffer;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, GL_ScreenTexture);

        if (GPU::Framebuffer[frontbuf][0] && GPU::Framebuffer[frontbuf][1]) {
            if (GPU3D::Renderer == 0) {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 192, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE,
                                GPU::Framebuffer[frontbuf][0]);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 192, 256, 192, GL_RGBA_INTEGER,
                                GL_UNSIGNED_BYTE, GPU::Framebuffer[frontbuf][1]);
            } else {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256 * 3 + 1, 192, GL_RGBA_INTEGER,
                                GL_UNSIGNED_BYTE, GPU::Framebuffer[frontbuf][0]);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 192, 256 * 3 + 1, 192, GL_RGBA_INTEGER,
                                GL_UNSIGNED_BYTE, GPU::Framebuffer[frontbuf][1]);
            }
        }

        glActiveTexture(GL_TEXTURE1);
        if (GPU3D::Renderer != 0)
            GPU3D::GLRenderer::SetupAccelFrame();

        glBindBuffer(GL_ARRAY_BUFFER, GL_ScreenVertexBufferID);
        glBindVertexArray(GL_ScreenVertexArrayID);
        glDrawArrays(GL_TRIANGLES, 0, 4 * 3);
    }

    refreshDisplay();
}

int main(int argc, char** argv) {
    initRenderer();
    Config::Load();

    // The old volume setting was scaled differently, so save upgrading users from
    // ear rape
    if (Config::AudioVolume > 4) {
        Config::AudioVolume = 4;
        Config::Save();
    }

    if (!fileBrowser()) {
        deinitRenderer();
        return 0;
    }

    if (!Platform::LocalFileExists("bios7.bin") || !Platform::LocalFileExists("bios9.bin") ||
        !Platform::LocalFileExists("firmware.bin")) {
        vector<string> message = {
            "One or more of the following required files don't exist or couldn't "
            "be accessed:",
            "bios7.bin -- ARM7 BIOS", "bios9.bin -- ARM9 BIOS", "firmware.bin -- firmware image",
            "Dump the files from your DS and place them in sdmc:/switch/melonds"};

        messageScreen("BIOS/Firmware not found", message, true);
        deinitRenderer();
        return 0;
    }

    micBuffer.next = NULL;
    micBuffer.buffer = new s16[(1440 * 2 + 0xFFF) & ~0xFFF];
    micBuffer.buffer_size = (1440 * 2 * sizeof(s16) + 0xFFF) & ~0xFFF;
    micBuffer.data_size = 1440 * 2 * sizeof(s16);
    micBuffer.data_offset = 0;

    GPU3D::GLRenderer::OpenGL_Init();
    GLScreen_Init();
    startCore(true);

    PadState pad;
    padInitializeDefault(&pad);

    HidTouchScreenState state = {0};

    while (appletMainLoop()) {
        chrono::steady_clock::time_point start = chrono::steady_clock::now();

		padUpdate(&pad);
        u64 pressed = padGetButtonsDown(&pad);
        u64 released = padGetButtonsUp(&pad);

        // Check for button input
        for (int i = 0; i < 12; i++) {
            if (pressed & Config::Mapping[i])
                NDS::PressKey(i > 9 ? i + 6 : i);
            else if (released & Config::Mapping[i])
                NDS::ReleaseKey(i > 9 ? i + 6 : i);
        }

        // Check for hotkey input
        for (int i = 12; i < 15; i++) {
            if (pressed & Config::Mapping[i])
                hotkeyMask |= BIT(i - 12);
            else if (released & Config::Mapping[i])
                hotkeyMask &= ~BIT(i - 12);
        }

        if (hotkeyMask & BIT(0)) // Lid close/open
        {
            lidClosed = !lidClosed;
            NDS::SetLidClosed(lidClosed);
            hotkeyMask &= ~BIT(0);
        } else if (hotkeyMask & BIT(2)) // Pause menu
        {
            if (!pauseMenu())
                break;
        }

        // Check for touch input
        if (false && hidGetTouchScreenStates(&state, 1) && state.count > 0) {
            HidTouchState *touch = &state.touches[0];
            //~ hidTouchRead(&touch, 0);

            if (touch->x > botX && touch->x < botX + botWidth && touch->y > botY &&
                touch->y < botY + botHeight) {
                int x, y;
                if (Config::ScreenRotation == 0) // 0
                {
                    x = (touch->x - botX) * 256.0f / botWidth;
                    y = (touch->y - botY) * 256.0f / botWidth;
                } else if (Config::ScreenRotation == 1) // 90
                {
                    x = (touch->y - botY) * 192.0f / botWidth;
                    y = 192 - (touch->x - botX) * 192.0f / botWidth;
                } else if (Config::ScreenRotation == 2) // 180
                {
                    x = (touch->x - botX) * -256.0f / botWidth;
                    y = 192 - (touch->y - botY) * 256.0f / botWidth;
                } else // 270
                {
                    x = (touch->y - botY) * -192.0f / botWidth;
                    y = (touch->x - botX) * 192.0f / botWidth;
                }
                NDS::PressKey(16 + 6);
                NDS::TouchScreen(x, y);
            }
        } else {
            NDS::ReleaseKey(16 + 6);
            NDS::ReleaseScreen();
        }

        NDS::RunFrame();
        GLScreen_DrawScreen();

        chrono::duration<double> elapsed = chrono::steady_clock::now() - start;
        if (Config::LimitFPS && elapsed.count() < 1.0f / 60)
            usleep((1.0f / 60 - elapsed.count()) * 1000000);
    }

    hotkeyMask |= BIT(2);
    pauseCore();
    deinitRenderer();
    return 0;
}
