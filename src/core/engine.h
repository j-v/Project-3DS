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

#ifndef P3DS_ENGINE_H
#define P3DS_ENGINE_H

#include <deque>
#include <vector>

#include "chart.h"
#include "field.h"
#include "input_mapper.h"
#include "results.h"

namespace p3ds {

/**
 * A nominal frame, in engine time units - derived from a single frame on the DS
 *
 * 1672 units is 16.72 ms, or 59.8086 Hz. The hit windows are multiples of this
 * value, so they cover fixed spans of real time.
 */
constexpr uint32_t NOMINAL_FRAME_TIME = 1672;

/** The judgement windows, in engine time units. */
constexpr int32_t WINDOW_COOL = (int32_t)(3 * NOMINAL_FRAME_TIME);  // 50.16 ms
constexpr int32_t WINDOW_FINE = (int32_t)(6 * NOMINAL_FRAME_TIME);  // 100.32 ms
constexpr int32_t WINDOW_SAD = (int32_t)(9 * NOMINAL_FRAME_TIME);   // 150.48 ms
constexpr int32_t WINDOW_MISS = (int32_t)(12 * NOMINAL_FRAME_TIME); // 200.64 ms

/**
 * The total-travel factor for a note.
 *
 * It controls how far from its hole a note starts (three seconds of travel
 * at 60 Hz), not how fast it moves. The speed comes from the note's flyTime,
 * so this stays a visual starting-distance factor.
 */
constexpr int32_t NOTE_START_TRAVEL = 60 * 3;

/** The judgement categories, from best to worst. */
enum Judgement
{
    JudgementCool = 0,
    JudgementFine = 1,
    JudgementSafe = 2,
    JudgementSad = 3,
    JudgementMiss = 4
};

/** A note that is on its way to a hole. */
struct Note
{
    uint8_t type = 0;
    int32_t x = 0;           // the hole, in screen pixels
    int32_t y = 0;
    int32_t startOfsX = 0;   // the travel at creation, in 24.8 fixed point
    int32_t startOfsY = 0;
    int32_t ofsX = 0;        // the current travel, in 24.8 fixed point
    int32_t ofsY = 0;
    uint16_t arrow = 0;      // the timing arrow's rotation, in 1/65536 turns
    uint32_t time = 0;       // when the note has to be hit
    uint32_t createTime = 0; // when the note was created
    uint32_t flyTime = 0;    // how long the note takes to reach its hole
};

/**
 * The gameplay core.
 *
 * The engine is pure logic: it never draws, reads files or talks to the
 * hardware. Time is handed to it by the caller, so the whole core can be
 * tested on a host with a fake clock and scripted input.
 */
class GameEngine
{
public:
    GameEngine();

    /** Loads a chart. The engine keeps a copy of it. */
    void setChart(const Chart &chart);

    /** Sets the geometry of the gameplay field. */
    void setField(const FieldConfig &field) { this->field = field; }

    /** Sets the difficulty, which decides how the hold score is weighted. */
    void setDifficulty(uint8_t difficulty);

    /** Restarts the song. */
    void reset();

    /**
     * Starts a frame at the given chart time.
     *
     * @param now  the current time, in engine time units
     * @param dt   how much time has passed since the previous frame
     */
    void beginFrame(uint32_t now, uint32_t dt);

    /**
     * Handles a frame's input.
     *
     * @param down  the actions that were pressed this frame
     * @param held  the actions that are currently held
     * @param up    the actions that were released this frame
     */
    void handleInput(ActionMask down, ActionMask held, ActionMask up);

    const Results &getResults() const { return results; }
    uint8_t currentLife() const { return life; }
    uint32_t currentCombo() const { return combo; }
    uint32_t score() const { return results.scoreBase + results.scoreHold + results.scoreSlide; }
    uint32_t referenceScore() const { return scoreRef; }
    bool isFinished() const { return finished; }
    bool failed() const { return life == 0; }
    bool hasMusicStarted() const { return musicStarted; }
    uint32_t currentLyric() const { return lyricIndex; }
    bool hasLyricLine() const { return hasLyric; }

    /** The notes that are currently in flight. */
    const std::deque<Note> &activeNotes() const { return notes; }

    /** The position of the judgement popup, if one is showing. */
    uint32_t judgementTimer() const { return statTimer; }
    uint8_t judgementType() const { return statType; }
    int32_t judgementX() const { return statCurX; }
    int32_t judgementY() const { return statCurY; }

    /** Whether two notes are part of the same multi-note. */
    static bool isMultiNote(const std::deque<Note> &notes, size_t index);

private:
    void createNote(const ChartEvent &event);
    void computeReferenceScore();
    bool judgeNotes(ActionMask down, ActionMask held, ActionMask up);
    void updateNotePositions();
    void updateHoldBonus(ActionMask held);

    Chart chart;
    FieldConfig field;

    std::deque<Note> notes;

    size_t eventIndex = 0;
    uint32_t currentTime = 0;
    uint32_t frameDelta = 0;
    uint32_t flyTime = 100000;
    bool finished = false;
    bool musicStarted = false;
    bool hasLyric = false;
    uint32_t lyricIndex = 0;

    uint32_t current = 0;
    uint8_t mask = 0;
    uint8_t mask2 = 0;

    uint32_t statTimer = 0;
    uint8_t statType = 0;
    int32_t statCurX = 0;
    int32_t statCurY = 0;
    int32_t statX = 0;
    int32_t statY = 0;

    uint8_t holdNotes = 0;
    uint8_t holdStart = 0;
    uint16_t holdTime = 0;
    uint16_t holdScore = 0;
    uint32_t holdFrameAccum = 0;
    uint32_t holdTimeAccum = 0;

    uint32_t slideCount = 0;
    bool slideBroken = false;

    uint32_t combo = 0;
    uint8_t life = 127;

    uint32_t scoreRef = 0;
    uint8_t holdDivide = 1;

    Results results;
};

} // namespace p3ds

#endif // P3DS_ENGINE_H
