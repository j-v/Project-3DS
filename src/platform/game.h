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

#ifndef P3DS_GAME_H
#define P3DS_GAME_H

#include <string>

#include "audio_engine.h"
#include "config.h"
#include "core/clock.h"
#include "core/engine.h"
#include "core/input_mapper.h"
#include "core/song_database.h"
#include "render_context.h"

namespace p3ds {

/** What a screen returned to the loop that drives the app. */
enum class FlowResult
{
    Resume,
    Retry,
    SongList
};

/** Why a song could not be started. */
enum class PlayResult
{
    Ok,
    ChartMissing,
    ChartUnreadable,
    AudioUnavailable
};

/**
 * The screens of the game.
 *
 * The song list, the pause menu and the results screen are drawn on the
 * bottom screen, and the gameplay is drawn on the top screen.
 */
class Game
{
public:
    Game(Config &config, SongDatabase &database, GameEngine &engine, AudioEngine &audio,
             RenderContext &render, InputMapper &mapper);

    /** Scans the chart files and sorts the song list. */
    void init();

    /** Runs the game until the user leaves it. */
    void run();

    /** Pauses the game when the HOME menu is opened. */
    void onSuspend();

    /** Starts the game again after the HOME menu was closed. */
    void onResume();

private:
    void songList();
    void changeDifficulty(int delta);
    PlayResult loadChart(const std::string &id, int difficulty);
    PlayResult playSong(const std::string &id, int difficulty);
    void showPlayError(PlayResult result, const std::string &id, int difficulty);
    FlowResult gameLoop();
    FlowResult pauseMenu(bool pause);
    FlowResult resultsScreen(bool fail);

    void drawGame();
    void drawSongList();
    void drawPauseMenu(bool canResume);
    void drawResults();
    void drawHud();

    /** The name of the song that is being played. */
    std::string currentId;
    int currentDifficulty = 1;

    bool running = true;
    bool sortAlphabetical = true;

    Config &config;
    SongDatabase &database;
    GameEngine &engine;
    AudioEngine &audio;
    RenderContext &render;
    InputMapper &mapper;

    int difficulty = 1;
    int selection = 0;
    int lagConfigMs = 0;
    int pauseItem = 0;

    TickClock clock;
    bool suspended = false;
    bool resumeMusicAfterSuspend = false;
};

} // namespace p3ds

#endif // P3DS_GAME_H
