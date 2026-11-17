/*
    Copyright 2022-2024 Hydr8gon
    Copyright 2026 Project 3DS contributors
    Modified 2026 by Project 3DS contributors.

    This file is part of Project 3DS, a derivative work of Project DS
    <https://github.com/Hydr8gon/Project-DS>.

    SPDX-License-Identifier: GPL-3.0-or-later
    This file is licensed under the GNU General Public License, version 3 or
    later. It is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See LICENSE for the full licence text.
*/

#include <3ds.h>

#include <cstdio>
#include <dirent.h>
#include <string>

#include "audio_engine.h"
#include "config.h"
#include "core/engine.h"
#include "core/input_mapper.h"
#include "core/song_database.h"
#include "game.h"
#include "platform.h"
#include "render_context.h"

using namespace p3ds;

/** Reads the song database and the saved scores. */
static void loadDatabase(SongDatabase &database, const Config &config)
{
    std::string dbPath = config.dataPath("db");

    DIR *dir = opendir(dbPath.c_str());

    if (dir)
    {
        while (dirent *entry = readdir(dir))
        {
            std::string name = entry->d_name;

            if (name.length() <= 4 || name.substr(name.length() - 4) != ".txt")
                continue;

            FILE *file = fopen((dbPath + "/" + name).c_str(), "r");

            if (!file)
                continue;

            std::string text;
            char line[512];

            while (fgets(line, sizeof(line), file))
                text += line;

            fclose(file);
            database.parseDatabaseText(text);
        }

        closedir(dir);
    }

    FILE *file = fopen(config.scoresPath().c_str(), "r");

    if (file)
    {
        std::string text;
        char line[512];

        while (fgets(line, sizeof(line), file))
            text += line;

        fclose(file);
        database.parseScoresText(text);
    }
}

#ifdef P3DS_DIAG
/** Shows what the clock self test measured. */
static void showClockReport(RenderContext &render, const ClockReport &report, bool new3DS)
{
    char buffer[128];

    render.beginTop();
    render.clearTop(BackgroundColour);
    render.drawText(16, 40, "Clock self test", 0xFFFFFFFF, 0.6f);

    snprintf(buffer, sizeof(buffer), "tick rate:   %.0f Hz", report.tickRate);
    render.drawText(16, 80, buffer);

    snprintf(buffer, sizeof(buffer), "expected:    %.0f Hz", (double)platform::TICK_RATE);
    render.drawText(16, 96, buffer);

    snprintf(buffer, sizeof(buffer), "display:     %.3f Hz", report.displayRate);
    render.drawText(16, 120, buffer);

    snprintf(buffer, sizeof(buffer), "expected:    59.831 Hz");
    render.drawText(16, 136, buffer);

    snprintf(buffer, sizeof(buffer), "frame:       %.0f ticks", report.displayPeriod);
    render.drawText(16, 160, buffer);

    snprintf(buffer, sizeof(buffer), "window:      %llu ms (RTC)",
             (unsigned long long)report.elapsedMs);
    render.drawText(16, 176, buffer);

    render.drawText(16, 196, new3DS ? "New 3DS" : "original 3DS");
    render.endTop();
}
#endif

/** The APT hook that pauses the game when the HOME menu is opened or the console sleeps. */
static void aptCallback(APT_HookType hook, void *param)
{
    Game *game = (Game *)param;

    switch (hook)
    {
        case APTHOOK_ONSUSPEND:
        case APTHOOK_ONSLEEP:
        {
            game->onSuspend();
            break;
        }

        case APTHOOK_ONRESTORE:
        case APTHOOK_ONWAKEUP:
        {
            game->onResume();
            break;
        }

        default:
        {
            break;
        }
    }
}

/** Writes a line to the diagnostic log of the SD card. */
#ifdef P3DS_DIAG
static void diagLog(const char *text)
{
    FILE *log = fopen("sdmc:/project-3ds-diag.log", "a");

    if (log)
    {
        fprintf(log, "%s\n", text);
        fclose(log);
    }
}
#else
static void diagLog(const char *text) { (void)text; }
#endif

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    diagLog("start");

    if (!platform::init())
        return 1;

    diagLog("platform ready");

    // A New 3DS is asked for its faster clock and its L2 cache, which is
    // best effort: the firmware is free to refuse
    platform::requestNew3DSPerformance();

    Config config;
    config.load();
    config.createFolders();

    // The database holds a thousand songs, so it is kept out of the stack of
    // the main thread
    static SongDatabase database;
    loadDatabase(database, config);

    RenderContext render;

    diagLog("setting the drawing up");

    if (!render.init())
    {
        platform::exit();
        return 1;
    }

    diagLog("the drawing is ready");

    AudioEngine audio;

    if (!audio.init())
    {
        render.exit();
        platform::exit();
        return 1;
    }

    // The buttons of the console are mapped to the actions of the game.
    // The D-pad is read as the D-pad alone, because the Circle Pad sits
    // next to the face buttons and would cause notes to be hit by accident.
    PhysicalButtons buttons;
    buttons.triangle = KEY_X;
    buttons.circle = KEY_A;
    buttons.cross = KEY_B;
    buttons.square = KEY_Y;
    buttons.dpadUp = KEY_DUP;
    buttons.dpadRight = KEY_DRIGHT;
    buttons.dpadDown = KEY_DDOWN;
    buttons.dpadLeft = KEY_DLEFT;
    buttons.shoulderLeft = KEY_L;
    buttons.shoulderRight = KEY_R;

    InputMapper mapper;
    mapper.setDefaults(buttons);

    GameEngine engine;
    Game game(config, database, engine, audio, render, mapper);
    game.init();

    aptHookCookie cookie;
    aptHook(&cookie, aptCallback, &game);

#ifdef P3DS_DIAG
    // The clock is measured, so that the timing of the console can be
    // checked
    diagLog("measuring the clock");

    ClockReport report = platform::measureClock(120);

    diagLog("the clock is measured, drawing the report");
    uint32_t frames = 0;

    while (aptMainLoop())
    {
        hidScanInput();

        render.beginFrame();
        showClockReport(render, report, platform::isNew3DS());
        render.endFrame();

        frames++;

        if (frames > 180 || (hidKeysDown() & KEY_START))
            break;
    }
#endif

    game.run();

    aptUnhook(&cookie);

    audio.exit();
    render.exit();
    platform::exit();

    return 0;
}
