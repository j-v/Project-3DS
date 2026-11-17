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

#include "game.h"

#include <3ds.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string>
#include <vector>

#include "core/clock.h"
#include "platform.h"

namespace p3ds {

/** The colours the difficulty tabs are shown in, as citro2d packs them. */
static const uint32_t DifficultyColours[5] =
{
    0xFFBD9C00, // Easy
    0xFF08B519, // Normal
    0xFF0094D6, // Hard
    0xFF2100DE, // Extreme
    0xFFE610AD  // Extra Extreme
};

/** The size of the gameplay field on the top screen. */
static const int FieldWidth = 400;
static const int FieldHeight = 240;

/** The bottom screen. */
static const int ScreenWidth = 320;
static const int ScreenHeight = 240;

/** How many songs are shown at once. */
static const int VisibleSongs = 7;

Game::Game(Config &config, SongDatabase &database, GameEngine &engine, AudioEngine &audio,
           RenderContext &render, InputMapper &mapper)
    : config(config), database(database), engine(engine), audio(audio), render(render),
      mapper(mapper), clock(platform::TICK_RATE)
{
}

void Game::init()
{
    // Scan the chart files and build the song list of every difficulty
    DIR *dir = opendir(config.dataPath("dsc").c_str());

    if (dir)
    {
        while (dirent *entry = readdir(dir))
            database.addChartFileName(entry->d_name);

        closedir(dir);
    }

    database.sortCharts(sortAlphabetical);
}

void Game::run()
{
    while (running && aptMainLoop())
        songList();
}

void Game::songList()
{
    // Make sure there is something to play at all. An empty difficulty is
    // handled by the list itself, so the player can switch to another tab.
    bool hasCharts = false;

    for (int i = 0; i < DifficultyCount; i++)
    {
        if (!database.charts(i).empty())
        {
            hasCharts = true;
            break;
        }
    }

    if (!hasCharts)
    {
        render.beginFrame();
        render.beginBottom();
        render.clearBottom(InterfaceBackgroundColour);
        render.drawText(16, 80, "No chart files were found.");
        render.drawText(16, 100, "Place the .dsc files in");
        render.drawText(16, 120, config.dataPath("dsc") + ".");
        render.endBottom();
        render.endFrame();

        while (running && aptMainLoop())
        {
            hidScanInput();

            // Start Key exits
            if (hidKeysDown() & KEY_START)
            {
                running = false;
                break;
            }
        }

        return;
    }

    // Keep the selection inside the current list; an empty tab has none
    const std::vector<std::string> &initialCharts = database.charts(difficulty);

    if (initialCharts.empty() || selection < 0)
        selection = 0;
    else if (selection >= (int)initialCharts.size())
        selection = (int)initialCharts.size() - 1;

    uint32_t frames = 0;
    bool preview = true;

    while (running && aptMainLoop())
    {
        hidScanInput();

        uint32_t down = hidKeysDown();
        uint32_t held = hidKeysHeld();
        bool scrolling = (held & (KEY_DUP | KEY_DDOWN)) != 0;

        const std::vector<std::string> &charts = database.charts(difficulty);

        render.beginFrame();
        drawSongList();
        render.endFrame();

        if (!scrolling)
            frames = 0;

        // The song that is selected is played as a preview
        if (preview && !scrolling && !charts.empty())
        {
            preview = false;
            audio.play(config.songPath(charts[selection]), 0);
        }

        if ((down & KEY_A) && !charts.empty())
        {
            // The song is played, and the list is left
            std::string id = charts[selection];
            int songDifficulty = difficulty;

            audio.stop();

            PlayResult result = playSong(id, songDifficulty);

            if (result != PlayResult::Ok)
                showPlayError(result, id, songDifficulty);

            return;
        }
        else if (down & KEY_Y)
        {
            // The way the songs are sorted is changed
            sortAlphabetical = !sortAlphabetical;
            database.sortCharts(sortAlphabetical);
            selection = 0;
            audio.stop();
            preview = true;
        }
        else if (down & KEY_DLEFT)
        {
            // The difficulty tab is moved to the left
            changeDifficulty(-1);
            audio.stop();
            preview = true;
        }
        else if (down & KEY_DRIGHT)
        {
            // The difficulty tab is moved to the right
            changeDifficulty(1);
            audio.stop();
            preview = true;
        }
        else if ((held & KEY_DUP) && !charts.empty())
        {
            // The selection is moved up, with wraparound
            if (frames > 30 || frames++ == 0)
            {
                if (--selection < 0)
                    selection = charts.size() - 1;

                audio.stop();
                preview = true;
            }
        }
        else if ((held & KEY_DDOWN) && !charts.empty())
        {
            // The selection is moved down, with wraparound
            if (frames > 30 || frames++ == 0)
            {
                if (++selection >= (int)charts.size())
                    selection = 0;

                audio.stop();
                preview = true;
            }
        }

        if (down & KEY_START)
        {
            running = false;
            break;
        }
    }
}

void Game::changeDifficulty(int delta)
{
    const std::vector<std::string> &current = database.charts(difficulty);
    std::string selectedId;
    int oldSelection = selection;

    if (selection >= 0 && selection < (int)current.size())
        selectedId = current[selection];

    if (oldSelection < 0)
        oldSelection = 0;

    int nextDifficulty = (difficulty + delta + DifficultyCount) % DifficultyCount;
    const std::vector<std::string> &next = database.charts(nextDifficulty);

    difficulty = nextDifficulty;

    // An empty tab has no selection to keep; the list shows its own message
    if (next.empty())
    {
        selection = 0;
        return;
    }

    std::vector<std::string>::const_iterator found = std::find(next.begin(), next.end(), selectedId);

    if (!selectedId.empty() && found != next.end())
        selection = (int)(found - next.begin());
    else
        selection = std::max(0, std::min(oldSelection, (int)next.size() - 1));
}

PlayResult Game::loadChart(const std::string &id, int songDifficulty)
{
    // The chart is loaded from the file
    std::string path = config.chartPath(id, songDifficulty);
    FILE *file = fopen(path.c_str(), "rb");

    if (!file)
        return PlayResult::ChartMissing;

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return PlayResult::ChartUnreadable;
    }

    long size = ftell(file);

    if (size <= 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return PlayResult::ChartUnreadable;
    }

    std::vector<uint8_t> data((size_t)size);
    size_t read = fread(&data[0], 1, data.size(), file);
    fclose(file);

    if (read != data.size())
        return PlayResult::ChartUnreadable;

    Chart chart = ChartLoader::parse(&data[0], data.size());

    // A chart that was cut off or has no end opcode would never finish
    if (!chart.valid || !chart.ended)
        return PlayResult::ChartUnreadable;

    engine.setChart(chart);
    engine.setField(FieldConfig());
    engine.setDifficulty(songDifficulty);
    return PlayResult::Ok;
}

PlayResult Game::playSong(const std::string &id, int songDifficulty)
{
    currentId = id;
    currentDifficulty = songDifficulty;

    // The chart is loaded once. A retry repeats the same chart, because the
    // file cannot change while the song is being played.
    PlayResult loaded = loadChart(id, songDifficulty);

    if (loaded != PlayResult::Ok)
        return loaded;

    // A retry goes around this loop instead of calling playSong again, so the
    // stack does not grow with every attempt. The engine is reset and the
    // music is prepared again, because the lag config may have changed.
    while (running)
    {
        engine.reset();

        // The first buffers of the song are decoded before the chart clock
        // starts, so the music can start on the exact frame the chart says
        if (!audio.prepare(config.songPath(id), lagConfigMs))
            return PlayResult::AudioUnavailable;

        if (gameLoop() != FlowResult::Retry)
            break;
    }

    return PlayResult::Ok;
}

void Game::showPlayError(PlayResult result, const std::string &id, int songDifficulty)
{
    std::string message;
    std::string path;

    switch (result)
    {
        case PlayResult::ChartMissing:
            message = "The chart file is missing:";
            path = config.chartPath(id, songDifficulty);
            break;

        case PlayResult::ChartUnreadable:
            message = "The chart file cannot be read:";
            path = config.chartPath(id, songDifficulty);
            break;

        case PlayResult::AudioUnavailable:
            message = "The music file is missing or cannot be decoded:";
            path = config.songPath(id);
            break;

        default:
            return;
    }

    while (running && aptMainLoop())
    {
        hidScanInput();

        render.beginFrame();
        render.beginBottom();
        render.clearBottom(InterfaceBackgroundColour);
        render.drawText(16, 80, message);

        // The path can be longer than the line, so it is split over two lines
        if (path.size() > 38)
        {
            render.drawText(16, 100, path.substr(0, 38));
            render.drawText(16, 116, path.substr(38));
        }
        else
        {
            render.drawText(16, 100, path);
        }

        render.drawText(16, 160, "Press A to return to the song list.");
        render.endBottom();
        render.endFrame();

        if (hidKeysDown() & KEY_A)
            break;

        if (hidKeysDown() & KEY_START)
        {
            running = false;
            break;
        }
    }
}

FlowResult Game::gameLoop()
{
    // The gameplay time starts when the song starts
    clock.reset(platform::ticks());
    suspended = false;

    uint32_t now = 0;
    uint32_t lastNow = 0;
    bool musicStarted = false;

    while (running && aptMainLoop())
    {
        hidScanInput();

        now += clock.advance(platform::ticks());
        uint32_t delta = now - lastNow;
        lastNow = now;

        engine.beginFrame(now, delta);

        uint32_t down = mapper.map(platform::pressedButtons());
        uint32_t held = mapper.map(platform::heldButtons());
        uint32_t up = mapper.map(platform::releasedButtons());

        engine.handleInput(down, held, up);

        // The music starts when the chart says so, which is the moment the
        // chart starts as well
        if (engine.hasMusicStarted() && !musicStarted)
        {
            musicStarted = true;
            audio.start();
        }

        render.beginFrame();
        drawGame();
        render.endFrame();

        if (platform::pressedButtons() & KEY_START)
        {
            FlowResult result = pauseMenu(true);

            if (result == FlowResult::Resume)
            {
                // The game stays where it was, and the music goes on where it
                // was paused
                clock.reset(platform::ticks());
                continue;
            }

            return result;
        }

        if (engine.currentLife() == 0 || (engine.isFinished() && engine.activeNotes().empty()))
        {
            bool fail = engine.currentLife() == 0;
            audio.stop();
            return resultsScreen(fail);
        }
    }

    return FlowResult::SongList;
}

FlowResult Game::pauseMenu(bool pause)
{
    // The music is paused while the game is paused, and stopped when the
    // results are shown
    if (pause)
        audio.pause();
    else
        audio.stop();

    // The game cannot be resumed once the lag config was changed, because
    // the music would be out of sync with the chart
    bool canResume = pause;

    pauseItem = pause ? 0 : 1;
    uint32_t frames = 0;

    while (running && aptMainLoop())
    {
        hidScanInput();

        uint32_t down = hidKeysDown();
        uint32_t held = hidKeysHeld();
        bool navigating = (held & (KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT)) != 0;

        // The frame is started before a screen is selected: the commands of a
        // scene that is selected outside of a frame are discarded
        render.beginFrame();
        render.beginBottom();
        render.clearBottom(InterfaceBackgroundColour);
        drawPauseMenu(canResume);
        render.endBottom();
        render.endFrame();

        if (!navigating)
            frames = 0;

        if (down & KEY_A)
        {
            switch (pauseItem)
            {
                case 0: // Resume the game
                    audio.resume();
                    return FlowResult::Resume;

                case 2: // The lag config
                    continue;

                case 3: // Back to the song list
                    audio.stop();
                    return FlowResult::SongList;

                case 1: // Try again
                {
                    audio.stop();
                    return FlowResult::Retry;
                }
            }
        }
        else if (held & KEY_DUP)
        {
            if (frames > 30 || frames++ == 0)
            {
                int lowest = canResume ? 0 : 1;

                if (--pauseItem < lowest)
                    pauseItem = 3;
            }
        }
        else if (held & KEY_DDOWN)
        {
            if (frames > 30 || frames++ == 0)
            {
                if (++pauseItem > 3)
                    pauseItem = canResume ? 0 : 1;
            }
        }
        else if (pauseItem == 2 && (held & KEY_DLEFT))
        {
            // The lag config is made smaller
            if ((frames > 30 || frames++ == 0) && lagConfigMs > -1000)
            {
                lagConfigMs--;
                canResume = false;
            }
        }
        else if (pauseItem == 2 && (held & KEY_DRIGHT))
        {
            // The lag config is made bigger
            if ((frames > 30 || frames++ == 0) && lagConfigMs < 1000)
            {
                lagConfigMs++;
                canResume = false;
            }
        }
    }

    return FlowResult::SongList;
}

FlowResult Game::resultsScreen(bool fail)
{
    const Results &results = engine.getResults();
    SongData &data = database.song(atoi(currentId.c_str()));

    uint8_t rank = SongDatabase::calculateRank(results, currentDifficulty, fail);

    // The results are saved if they are better than the ones that are saved
    bool update = false;

    if (engine.score() > data.scores[currentDifficulty])
        update = true, data.scores[currentDifficulty] = engine.score();

    if (results.clear > data.clears[currentDifficulty])
        update = true, data.clears[currentDifficulty] = results.clear;

    if (rank > data.ranks[currentDifficulty])
        update = true, data.ranks[currentDifficulty] = rank;

    if (update)
    {
        FILE *file = fopen(config.scoresPath().c_str(), "w");

        if (file)
        {
            std::string text = database.serializeScores();
            fwrite(text.c_str(), 1, text.size(), file);
            fclose(file);
        }
    }

    while (running && aptMainLoop())
    {
        hidScanInput();

        // The frame is started before a screen is selected, the same way as
        // the pause menu, or nothing is drawn
        render.beginFrame();
        render.beginBottom();
        render.clearBottom(InterfaceBackgroundColour);
        drawResults();
        render.endBottom();
        render.endFrame();

        if (hidKeysDown() & KEY_A)
            break;
    }

    return pauseMenu(false);
}

void Game::onSuspend()
{
    // The game and the music are paused while the HOME menu is open or the
    // console sleeps. The music only starts again afterwards if it was not
    // already paused by the pause menu. The two suspend paths can overlap, so
    // a second one must not overwrite the state of the first.
    if (!suspended)
    {
        suspended = true;
        resumeMusicAfterSuspend = audio.isPlaying() && !audio.isPaused();
    }

    audio.pause();
}

void Game::onResume()
{
    suspended = false;

    // The gameplay time does not move while the game is paused
    clock.reset(platform::ticks());

    if (resumeMusicAfterSuspend)
        audio.resume();

    resumeMusicAfterSuspend = false;
}

void Game::drawHud()
{
    const Results &results = engine.getResults();

    // The score and the clear percentage share the right end of the row, so
    // the gauge can fill everything else
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%07lu", (unsigned long)engine.score());
    float scoreWidth = render.textWidth(buffer);

    char percentage[64];
    snprintf(percentage, sizeof(percentage), "%.02f%%", results.clear);
    float percentageLeft = ScreenWidth - 8 - render.textWidth(percentage);
    float scoreLeft = percentageLeft - 12 - scoreWidth;

    // The life gauge takes the width between the label and the score, with
    // a small padding on each side
    float labelWidth = render.textWidth("LIFE");
    float gaugeLeft = 8 + labelWidth + 8;
    float gaugeWidth = scoreLeft - 8 - gaugeLeft;

    render.drawText(8, 4, "LIFE");
    render.drawTextureScaled(TextureLifeEmpty, gaugeLeft, 6, gaugeWidth, 8);

    if (engine.currentLife() > 0)
    {
        float width = (float)engine.currentLife() / 255.0f * gaugeWidth;
        render.drawTextureScaled(TextureLifeFull, gaugeLeft, 6, width, 8);
    }

    render.drawText(scoreLeft, 4, buffer);
    render.drawText(percentageLeft, 4, percentage);
}

void Game::drawGame()
{
    // The top screen holds the gameplay
    render.beginTop();
    render.clearTop(BackgroundColour);

    const std::deque<Note> &notes = engine.activeNotes();

    // The holes of the notes are drawn at a depth behind the notes, so an
    // approaching note covers its hole instead of the other way round
    for (size_t i = 0; i < notes.size(); i++)
    {
        const Note &note = notes[i];
        bool multi = GameEngine::isMultiNote(notes, i);

        if (note.type & NOTE_SLIDE_CONT)
        {
            Texture hole = (note.type & 0xF) == 4 ? TextureSliderLeftHeldHole : TextureSliderRightHeldHole;
            if (multi)
                hole = (note.type & 0xF) == 4 ? TextureMultiSliderLeftHeldHole : TextureMultiSliderRightHeldHole;

            render.drawTexture(hole, note.x, note.y, 0.0f, 0.2f);
        }
        else
        {
            Texture hole = TextureTriangleHole;
            Texture multiHole = TextureMultiTriangleHole;

            switch (note.type & 0xF)
            {
                case 0: hole = TextureTriangleHole; multiHole = TextureMultiTriangleHole; break;
                case 1: hole = TextureCircleHole; multiHole = TextureMultiCircleHole; break;
                case 2: hole = TextureCrossHole; multiHole = TextureMultiCrossHole; break;
                case 3: hole = TextureSquareHole; multiHole = TextureMultiSquareHole; break;
                case 4: hole = TextureSliderLeftHole; multiHole = TextureMultiSliderLeftHole; break;
                case 5: hole = TextureSliderRightHole; multiHole = TextureMultiSliderRightHole; break;
                default: break;
            }

            render.drawTexture(multi ? multiHole : hole, note.x, note.y, 0.0f, 0.2f);

            // The timing arrow shows how much time is left until the note
            // has to be hit
            render.drawTextureCentered(TextureArrow, note.x + 16, note.y + 16,
                                       (float)note.arrow * 6.2831853f / 65536.0f, 0.4f);

            // A held note is shown with a bar
            if (note.type & NOTE_HELD)
                render.drawTexture(TextureHold, note.x, note.y + 20, 0.0f, 0.45f);
        }
    }

    // The notes themselves
    for (size_t i = 0; i < notes.size(); i++)
    {
        const Note &note = notes[i];
        bool multi = GameEngine::isMultiNote(notes, i);

        Texture image = TextureTriangle;

        if (note.type & NOTE_SLIDE_CONT)
            image = (note.type & 0xF) == 4 ? TextureSliderLeftHeld : TextureSliderRightHeld;
        else
        {
            switch (note.type & 0xF)
            {
                case 0: image = TextureTriangle; break;
                case 1: image = TextureCircle; break;
                case 2: image = TextureCross; break;
                case 3: image = TextureSquare; break;
                case 4: image = TextureSliderLeft; break;
                case 5: image = TextureSliderRight; break;
                default: break;
            }
        }

        if (multi)
        {
            switch (image)
            {
                case TextureTriangle: image = TextureMultiTriangle; break;
                case TextureCircle: image = TextureMultiCircle; break;
                case TextureCross: image = TextureMultiCross; break;
                case TextureSquare: image = TextureMultiSquare; break;
                case TextureSliderLeft: image = TextureMultiSliderLeft; break;
                case TextureSliderRight: image = TextureMultiSliderRight; break;
                case TextureSliderLeftHeld: image = TextureMultiSliderLeftHeld; break;
                case TextureSliderRightHeld: image = TextureMultiSliderRightHeld; break;
                default: break;
            }
        }

        int x = note.x + (note.ofsX >> 8);
        int y = note.y + (note.ofsY >> 8);

        if (x > -32 && x < FieldWidth && y > -32 && y < FieldHeight)
            render.drawTexture(image, x, y, 0.0f, 0.55f);
    }

    // The judgement of the last note is shown next to it
    if (engine.judgementTimer() > 0)
    {
        Texture judgement = TextureCool;

        switch (engine.judgementType())
        {
            case JudgementFine: judgement = TextureFine; break;
            case JudgementSafe: judgement = TextureSafe; break;
            case JudgementSad: judgement = TextureSad; break;
            case JudgementMiss: judgement = TextureMiss; break;
            default: judgement = TextureCool; break;
        }

        uint32_t combo = engine.currentCombo();
        int32_t x = 32;
        float popupX = engine.judgementX() + x - 32;

        if (combo > 1)
        {
            int digits = 0;
            for (uint32_t c = combo; c > 0; c /= 10)
                digits++;

            // Place the popup to the left of the combo digits. The 7-pixel
            // advance centres it on the digits; the rendered width is measured
            // below to right-align the text.
            x += 7 * digits / 2;

            char buffer[16];
            snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)combo);

            // A glyph sits a few pixels down in the font's line box, so raise
            // the text to keep the top of the digits level with the popup
            float comboLeft = engine.judgementX() + x + 1 - render.textWidth(buffer);
            render.drawText(comboLeft, engine.judgementY() - 3.0f, buffer, 0xFFFFFFFF, 0.4f, 0.65f);
            popupX = comboLeft - 32;
        }

        render.drawTexture(judgement, popupX, engine.judgementY(), 0.0f, 0.6f);
    }

    render.endTop();

    // The bottom screen holds the interface
    render.beginBottom();
    render.clearBottom(InterfaceBackgroundColour);
    drawHud();

    // The lyric of the song, just under the life gauge
    if (engine.hasLyricLine())
    {
        const std::vector<std::string> &lyrics = database.song(atoi(currentId.c_str())).lyrics;

        if (engine.currentLyric() < lyrics.size())
        {
            const std::string &lyric = lyrics[engine.currentLyric()];

            if (lyric.length() > 38)
            {
                // A long lyric is shown on two lines
                size_t split = lyric.substr(0, 38).find_last_of(" ");
                if (split == std::string::npos)
                    split = 38;

                render.drawTextCentered(ScreenWidth / 2, 18, lyric.substr(0, split));
                render.drawTextCentered(ScreenWidth / 2, 32, lyric.substr(split + 1));
            }
            else
            {
                render.drawTextCentered(ScreenWidth / 2, 18, lyric);
            }
        }
    }

    render.endBottom();
}

void Game::drawSongList()
{
    const std::vector<std::string> &charts = database.charts(difficulty);

    render.beginBottom();
    render.clearBottom(InterfaceBackgroundColour);

    // The offset of the first song that is shown
    size_t offset = 0;

    if (charts.size() > VisibleSongs)
    {
        if (selection >= (int)charts.size() - 4)
            offset = charts.size() - VisibleSongs;
        else if (selection > 2)
            offset = selection - 2;
    }

    for (size_t i = offset; i < offset + std::min(charts.size(), (size_t)VisibleSongs); i++)
    {
        SongData &data = database.song(atoi(charts[i].c_str()));
        bool selected = ((int)i == selection);
        uint32_t colour = selected ? 0xFFFFFFFF : 0xFFC0B8B0;

        float y = 6 + (float)(i - offset) * 28.0f;

        render.drawText(8, y, data.name.substr(0, 26), colour);

        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%4.1f", (float)((data.difficulties[difficulty] & 0x1F)) / 2.0f);
        render.drawText(ScreenWidth - 8 - render.textWidth(buffer), y, buffer, colour);

        snprintf(buffer, sizeof(buffer), "%07lu", (unsigned long)data.scores[difficulty]);
        render.drawText(60, y + 14, buffer, colour);

        snprintf(buffer, sizeof(buffer), "%6.2f%%", data.clears[difficulty]);
        render.drawText(160, y + 14, buffer, colour);

        render.drawText(240, y + 14, SongDatabase::rankName(data.ranks[difficulty]), colour);
    }

    if (charts.empty())
        render.drawTextCentered(ScreenWidth / 2, 100, "No songs in this difficulty.");

    // The difficulty tabs
    static const char *tabNames[5] = { "Easy", "Normal", "Hard", "Extrm", "ExEx" };
    float x = 8;

    for (int i = 0; i < 5; i++)
    {
        uint32_t colour = (i == difficulty) ? DifficultyColours[i] : 0xFFA09890;
        render.drawText(x, ScreenHeight - 18, (i == difficulty ? "> " : "  ") + std::string(tabNames[i]), colour);
        x += render.textWidth(tabNames[i]) + 18;
    }

    render.endBottom();
}

void Game::drawPauseMenu(bool canResume)
{
    static const char *items[4] = { "Resume Game", "Retry", "Lag Config", "Return to Song List" };

    for (int i = 0; i < 4; i++)
    {
        // The game cannot be resumed once the lag config was changed
        if (i == 0 && !canResume)
            continue;

        float y = 60 + (float)i * 24.0f;
        render.drawText(60, y, (i == pauseItem ? "> " : "  ") + std::string(items[i]));
    }

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "<%05d>", lagConfigMs);
    render.drawText(180, 108, buffer);
}

void Game::drawResults()
{
    const Results &results = engine.getResults();
    SongData &data = database.song(atoi(currentId.c_str()));

    char buffer[128];

    // The song that was played
    snprintf(buffer, sizeof(buffer), "%s - %s", SongDatabase::difficultyName(currentDifficulty),
             data.name.c_str());
    render.drawText(8, 6, buffer);

    uint8_t rank = SongDatabase::calculateRank(results, currentDifficulty, engine.currentLife() == 0);

    // The clear banner is cyan for a perfect play and red when it was not
    // cleared, and green otherwise
    if (rank == 4)
        render.drawTextCentered(ScreenWidth / 2, 34, "CLEAR!", 0xFF66FFFF);
    else if (rank == 0)
        render.drawTextCentered(ScreenWidth / 2, 34, "NOT CLEAR...", 0xFF6666FF);
    else
        render.drawTextCentered(ScreenWidth / 2, 34, "CLEAR!", 0xFF66FF66);

    const float tableWidth = 202;
    const float labelX = ((float)ScreenWidth - tableWidth) / 2.0f;
    const float centerRowRight = labelX + 112;
    const float rightRowRight = labelX + tableWidth;
    const float tableTop = 56;
    const float rowHeight = 14;
    const float headerGap = 8;
    const float sectionGap = 4;
    const float rowsTop = tableTop + rowHeight + headerGap;

    // Draws a value against the right edge of a column
    auto drawValue = [&](float right, float y, const char *text)
    {
        render.drawText(right - render.textWidth(text), y, text);
    };

    // The rank and clear percentage are the first row of the table
    static const char *rankNames[5] = { "DROPxOUT", "STANDARD", "GREAT", "EXCELLENT", "PERFECT" };
    render.drawText(labelX, tableTop, rankNames[rank]);

    snprintf(buffer, sizeof(buffer), "%.2f%%", results.clear);
    drawValue(rightRowRight, tableTop, buffer);

    // The judgements, which are named by their icons
    static const Texture judgements[5] = { TextureCool, TextureFine, TextureSafe, TextureSad, TextureMiss };
    uint32_t counts[5] = { results.cools, results.fines, results.safes, results.sads, results.misses };

    for (int i = 0; i < 5; i++)
    {
        float y = rowsTop + rowHeight * i;
        render.drawTexture(judgements[i], labelX, y + 2);

        snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)counts[i]);
        drawValue(centerRowRight, y, buffer);

        snprintf(buffer, sizeof(buffer), "%.2f%%",
                 results.total ? 100.0f * counts[i] / results.total : 0.0f);
        drawValue(rightRowRight, y, buffer);
    }

    // The rest of the statistics
    static const char *statistics[4] = { "COMBO", "HOLD", "SLIDE", "SCORE" };
    uint32_t values[4] = { results.comboMax, results.scoreHold, results.scoreSlide, engine.score() };

    for (int i = 0; i < 4; i++)
    {
        // The score is set apart from the statistics above it
        float y = rowsTop + rowHeight * (5 + i) + sectionGap + (i == 3 ? sectionGap : 0.0f);
        render.drawText(labelX, y, statistics[i]);

        snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)values[i]);
        drawValue(rightRowRight, y, buffer);
    }
}

} // namespace p3ds
