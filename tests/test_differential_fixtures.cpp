/*
    Copyright 2026 Project 3DS contributors

    SPDX-License-Identifier: GPL-3.0-or-later
    This file is licensed under the GNU General Public License, version 3 or
    later. It is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See LICENSE for the full licence text.
*/

/*
    The frozen differential fixtures.

    The outcomes in tests/fixtures/ were recorded from the reference engine
    with the libnds shim that lives on the archive/differential-tests branch.
    This test replays the same charts and the same scripted input through this
    engine and compares the result.

    The fixtures cannot discover a difference outside the recorded scenarios,
    but they catch a regression in judgement, scoring, life, combo or note
    geometry without needing the reference sources or a sibling checkout.
*/

#include "test_framework.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "core/chart.h"
#include "core/engine.h"
#include "differential_cases.h"
#include "ds_field.h"
#include "scenario.h"

#ifndef P3DS_SOURCE_DIR
#define P3DS_SOURCE_DIR "."
#endif

namespace {

const int MaxFrames = 200000;

/** What the reference engine produced, as recorded in the fixture. */
struct Fixture
{
    int difficulty = 1;
    bool completed = false;
    bool failed = false;
    uint32_t total = 0;
    uint32_t cools = 0;
    uint32_t fines = 0;
    uint32_t safes = 0;
    uint32_t sads = 0;
    uint32_t misses = 0;
    uint32_t comboMax = 0;
    uint32_t scoreBase = 0;
    uint32_t scoreHold = 0;
    uint32_t scoreSlide = 0;
    uint32_t life = 127;
    float clear = 0;
};

/** What this engine produced. */
struct Outcome
{
    uint32_t total = 0;
    uint32_t cools = 0;
    uint32_t fines = 0;
    uint32_t safes = 0;
    uint32_t sads = 0;
    uint32_t misses = 0;
    uint32_t comboMax = 0;
    uint32_t scoreBase = 0;
    uint32_t scoreHold = 0;
    uint32_t scoreSlide = 0;
    float clear = 0;
    uint8_t life = 127;
    bool failed = false;
};

std::string fixturePath(const char *name)
{
    return std::string(P3DS_SOURCE_DIR) + "/tests/fixtures/" + name;
}

void trim(std::string &line)
{
    while (!line.empty() && (line[line.size() - 1] == '\n' || line[line.size() - 1] == '\r'))
        line.erase(line.size() - 1);
}

std::map<std::string, Fixture> loadDifferentialFixtures()
{
    std::map<std::string, Fixture> fixtures;
    FILE *file = fopen(fixturePath("differential.txt").c_str(), "r");

    if (!file)
        return fixtures;

    char buffer[256];
    Fixture *current = nullptr;

    while (fgets(buffer, sizeof(buffer), file))
    {
        std::string line = buffer;
        trim(line);

        if (line.empty() || line[0] == '#')
            continue;

        if (line[0] == '[' && line[line.size() - 1] == ']')
        {
            current = &fixtures[line.substr(1, line.size() - 2)];
            continue;
        }

        if (!current)
            continue;

        size_t equals = line.find('=');

        if (equals == std::string::npos)
            continue;

        std::string key = line.substr(0, equals);
        std::string value = line.substr(equals + 1);

        if (key == "difficulty")
            current->difficulty = atoi(value.c_str());
        else if (key == "completed")
            current->completed = atoi(value.c_str()) != 0;
        else if (key == "failed")
            current->failed = atoi(value.c_str()) != 0;
        else if (key == "total")
            current->total = (uint32_t)strtoul(value.c_str(), nullptr, 10);
        else if (key == "cools")
            current->cools = (uint32_t)strtoul(value.c_str(), nullptr, 10);
        else if (key == "fines")
            current->fines = (uint32_t)strtoul(value.c_str(), nullptr, 10);
        else if (key == "safes")
            current->safes = (uint32_t)strtoul(value.c_str(), nullptr, 10);
        else if (key == "sads")
            current->sads = (uint32_t)strtoul(value.c_str(), nullptr, 10);
        else if (key == "misses")
            current->misses = (uint32_t)strtoul(value.c_str(), nullptr, 10);
        else if (key == "comboMax")
            current->comboMax = (uint32_t)strtoul(value.c_str(), nullptr, 10);
        else if (key == "scoreBase")
            current->scoreBase = (uint32_t)strtoul(value.c_str(), nullptr, 10);
        else if (key == "scoreHold")
            current->scoreHold = (uint32_t)strtoul(value.c_str(), nullptr, 10);
        else if (key == "scoreSlide")
            current->scoreSlide = (uint32_t)strtoul(value.c_str(), nullptr, 10);
        else if (key == "life")
            current->life = (uint32_t)strtoul(value.c_str(), nullptr, 10);
        else if (key == "clear")
            current->clear = strtof(value.c_str(), nullptr);
    }

    fclose(file);
    return fixtures;
}

Outcome runPort(const std::vector<uint8_t> &chartBytes, const std::vector<uint32_t> &trace,
                int difficulty)
{
    p3ds::Chart chart = p3ds::ChartLoader::parse(chartBytes.data(), chartBytes.size());

    p3ds::GameEngine engine;
    engine.setChart(chart);
    engine.setField(testing::dsFieldConfig());
    engine.setDifficulty(difficulty);

    Outcome out;
    uint32_t previousHeld = 0;

    for (int frame = 0; frame < MaxFrames; frame++)
    {
        uint32_t held = (size_t)frame < trace.size() ? trace[frame] : 0;
        uint32_t down = held & ~previousHeld;
        uint32_t up = previousHeld & ~held;
        previousHeld = held;

        engine.beginFrame(frame * scenario::FrameTime, scenario::FrameTime);
        engine.handleInput(down, held, up);

        if (engine.currentLife() == 0)
        {
            out.failed = true;
            break;
        }

        if (engine.isFinished() && engine.activeNotes().empty())
            break;
    }

    const p3ds::Results &results = engine.getResults();

    out.total = results.total;
    out.cools = results.cools;
    out.fines = results.fines;
    out.safes = results.safes;
    out.sads = results.sads;
    out.misses = results.misses;
    out.comboMax = results.comboMax;
    out.scoreBase = results.scoreBase;
    out.scoreHold = results.scoreHold;
    out.scoreSlide = results.scoreSlide;
    out.clear = results.clear;
    out.life = engine.currentLife();

    return out;
}

void checkEqual(const char *scenarioName, const char *what, long long expected, long long actual)
{
    if (expected != actual)
    {
        char message[256];
        snprintf(message, sizeof(message), "%s: %s differs: expected %lld, got %lld",
                 scenarioName, what, expected, actual);
        testing::reportFailure(__FILE__, __LINE__, message);
    }
}

void checkNear(const char *scenarioName, const char *what, double expected, double actual)
{
    if (!testing::nearlyEqual(expected, actual, 0.005))
    {
        char message[256];
        snprintf(message, sizeof(message), "%s: %s differs: expected %f, got %f",
                 scenarioName, what, expected, actual);
        testing::reportFailure(__FILE__, __LINE__, message);
    }
}

struct Position
{
    int frame;
    int x;
    int y;
};

std::vector<Position> loadPositionFixtures()
{
    std::vector<Position> positions;
    FILE *file = fopen(fixturePath("positions.txt").c_str(), "r");

    if (!file)
        return positions;

    char buffer[256];

    while (fgets(buffer, sizeof(buffer), file))
    {
        if (buffer[0] == '#')
            continue;

        Position position;

        if (sscanf(buffer, "%d %d %d", &position.frame, &position.x, &position.y) == 3)
            positions.push_back(position);
    }

    fclose(file);
    return positions;
}

/** Where a note moves in one frame, in pixels. */
double frameTravel(const p3ds::Note &note)
{
    double pixels = (double)(note.startOfsY >> 8);
    if (pixels < 0)
        pixels = -pixels;
    return pixels * 1666.67 / (double)note.flyTime;
}

} // namespace

TEST(differential_fixtures_match_the_reference)
{
    std::map<std::string, Fixture> fixtures = loadDifferentialFixtures();
    const std::vector<diffcases::Case> &cases = diffcases::all();

    CHECK(!fixtures.empty());
    CHECK_EQ(fixtures.size(), cases.size());

    for (size_t i = 0; i < cases.size(); i++)
    {
        const diffcases::Case &scenarioCase = cases[i];
        std::map<std::string, Fixture>::const_iterator it = fixtures.find(scenarioCase.name);

        CHECK(it != fixtures.end());

        if (it == fixtures.end())
            continue;

        const Fixture &expected = it->second;
        Outcome actual = runPort(scenarioCase.chart, scenarioCase.trace, scenarioCase.difficulty);

        checkEqual(scenarioCase.name, "difficulty", expected.difficulty, scenarioCase.difficulty);
        checkEqual(scenarioCase.name, "completed", expected.completed, 1);
        checkEqual(scenarioCase.name, "failed", expected.failed, actual.failed);
        checkEqual(scenarioCase.name, "total", expected.total, actual.total);
        checkEqual(scenarioCase.name, "cools", expected.cools, actual.cools);
        checkEqual(scenarioCase.name, "fines", expected.fines, actual.fines);
        checkEqual(scenarioCase.name, "safes", expected.safes, actual.safes);
        checkEqual(scenarioCase.name, "sads", expected.sads, actual.sads);
        checkEqual(scenarioCase.name, "misses", expected.misses, actual.misses);
        checkEqual(scenarioCase.name, "comboMax", expected.comboMax, actual.comboMax);
        checkEqual(scenarioCase.name, "scoreBase", expected.scoreBase, actual.scoreBase);
        checkEqual(scenarioCase.name, "scoreHold", expected.scoreHold, actual.scoreHold);
        checkEqual(scenarioCase.name, "scoreSlide", expected.scoreSlide, actual.scoreSlide);
        checkEqual(scenarioCase.name, "life", expected.life, actual.life);
        checkNear(scenarioCase.name, "clear", expected.clear, actual.clear);
    }
}

TEST(note_positions_match_the_reference_fixture)
{
    const diffcases::PositionCase &position = diffcases::position();
    std::vector<Position> fixtures = loadPositionFixtures();

    CHECK(!fixtures.empty());

    std::map<int, Position> byFrame;

    for (size_t i = 0; i < fixtures.size(); i++)
        byFrame[fixtures[i].frame] = fixtures[i];

    p3ds::Chart chart = p3ds::ChartLoader::parse(position.chart.data(), position.chart.size());

    p3ds::GameEngine engine;
    engine.setChart(chart);
    engine.setField(testing::dsFieldConfig());
    engine.setDifficulty(1);

    size_t compared = 0;
    uint32_t previousHeld = 0;

    for (int frame = 0; frame < 120; frame++)
    {
        uint32_t held = 0;
        uint32_t down = held & ~previousHeld;
        uint32_t up = previousHeld & ~held;
        previousHeld = held;

        engine.beginFrame(frame * scenario::FrameTime, scenario::FrameTime);
        engine.handleInput(down, held, up);

        std::map<int, Position>::const_iterator it = byFrame.find(frame);

        if (it == byFrame.end() || engine.activeNotes().empty())
            continue;

        const Position &expected = it->second;
        const p3ds::Note &note = engine.activeNotes().front();
        double tolerance = frameTravel(note) + 2.0;

        int portedX = note.x + (note.ofsX >> 8);
        int portedY = note.y + (note.ofsY >> 8);

        CHECK_NEAR(expected.x, portedX, tolerance);
        CHECK_NEAR(expected.y, portedY, tolerance);
        compared++;
    }

    // The test has to have compared something
    CHECK(compared > 10);
}

TEST(reference_score_matches_the_ds_build)
{
    std::vector<uint8_t> types;

    for (int i = 0; i < 60; i++)
        types.push_back((uint8_t)(i % 4));

    scenario::ChartBuilder builder;
    builder.targetFlyingTime(diffcases::FlyTime / 100);

    for (size_t i = 0; i < types.size(); i++)
    {
        builder.time((uint32_t)(i * diffcases::FlyTime));
        builder.target(types[i], 240000, 135000, 0, 100000);
    }

    builder.end();

    std::vector<uint8_t> bytes = builder.bytes();
    p3ds::Chart chart = p3ds::ChartLoader::parse(bytes.data(), bytes.size());
    p3ds::GameEngine engine;
    engine.setChart(chart);

    // 60 notes, each worth 750 points, less the 7250-point reduction for
    // combos below 50
    CHECK_EQ(engine.referenceScore(), 60u * 750u - 7250u);
    CHECK_EQ(engine.getResults().total, 60u);
}
