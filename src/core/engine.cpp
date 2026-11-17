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

#include "engine.h"

#include <algorithm>
#include <cmath>

namespace p3ds {

// The engine turns a note's low nibble into an action bit, so the two enums
// have to keep the same button order.
static_assert(ActionTriangle == 0 && ActionCircle == 1 && ActionCross == 2 &&
              ActionSquare == 3 && ActionSlideLeft == 4 && ActionSlideRight == 5,
              "action bits must match the chart's note button values");

#define PI 3.14159

GameEngine::GameEngine()
{
    setField(FieldConfig());
}

void GameEngine::setChart(const Chart &newChart)
{
    chart = newChart;
    reset();
}

void GameEngine::setDifficulty(uint8_t difficulty)
{
    // The hold score is multiplied by 4 and divided by this for the clear
    // percentage
    static const uint8_t divides[] = { 1, 2, 8, 20, 20 };
    holdDivide = divides[difficulty < 5 ? difficulty : 4];
}

void GameEngine::reset()
{
    notes.clear();
    eventIndex = 0;
    currentTime = 0;
    frameDelta = 0;
    flyTime = 100000;
    finished = false;
    musicStarted = false;
    hasLyric = false;
    lyricIndex = 0;
    current = 0;
    mask = 0;
    mask2 = 0;
    statTimer = 0;
    statType = 0;
    statCurX = 0;
    statCurY = 0;
    statX = 0;
    statY = 0;
    holdNotes = 0;
    holdStart = 0;
    holdTime = 0;
    holdScore = 0;
    holdFrameAccum = 0;
    holdTimeAccum = 0;
    slideCount = 0;
    slideBroken = false;
    combo = 0;
    life = 127;
    results = Results();

    computeReferenceScore();
}

void GameEngine::computeReferenceScore()
{
    uint32_t score = 0;
    bool multi = false;
    bool havePrev = false;
    uint32_t prevGroup = 0;

    // Scan the whole chart and add up the reference score and total combo
    for (size_t i = 0; i < chart.events.size(); i++)
    {
        const ChartEvent &event = chart.events[i];

        if (event.type != ChartEventType::Target)
            continue;

        // A TIME opcode ends a group of notes, and the first note of a group
        // is the only one that counts towards the combo
        if (!havePrev || event.target.group != prevGroup)
            multi = false;

        // Only the continuation of a held slide keeps its type here; every
        // other note counts as a plain note
        uint8_t type = 0;
        if (event.target.noteType & NOTE_SLIDE_CHAIN)
            type = event.target.noteType;

        // Add a score bonus and increment the total combo for combo-able notes
        if (!multi && !(type & NOTE_SLIDE_CONT))
        {
            score += 250;
            results.total++;
        }

        // Increase the reference score
        score += 500;
        multi = true;
        prevGroup = event.target.group;
        havePrev = true;
    }

    // Set the reference score, adjusted for combos below 50
    scoreRef = score - 7250;
}

void GameEngine::createNote(const ChartEvent &event)
{
    const ChartTarget &target = event.target;

    // Get the note angle and travel distance, scaled
    float angle = (float)(int32_t)target.angle * PI / 180000;
    float distanceX = (float)target.distance * 0x100 / field.travelDivX;
    float distanceY = (float)target.distance * 0x100 / field.travelDivY;

    Note note;
    note.type = target.noteType;
    note.x = chartToScreenX(target.x, field);
    note.y = chartToScreenY(target.y, field);

    // Calculate the positional offset the note starts its flight with. A note
    // starts 180 frames of travel from its hole; this is a visual distance,
    // not a speed.
    note.startOfsX = (int)(sin(angle) * distanceX) * NOTE_START_TRAVEL;
    note.startOfsY = (int)(cos(angle) * -distanceY) * NOTE_START_TRAVEL;
    note.ofsX = note.startOfsX;
    note.ofsY = note.startOfsY;

    // Add a note to the queue
    note.time = currentTime + flyTime;
    note.createTime = currentTime;
    note.flyTime = flyTime;
    note.arrow = 0;
    notes.push_back(note);
}

void GameEngine::updateNotePositions()
{
    for (Note &note : notes)
    {
        uint32_t elapsed = (currentTime > note.createTime) ? currentTime - note.createTime : 0;
        float frac = note.flyTime ? (float)elapsed / (float)note.flyTime : 0.0f;

        // The note's flight is parameterised by time, so it reaches its hole
        // exactly when it has to be hit
        note.ofsX = (int32_t)(note.startOfsX * (1.0f - frac));
        note.ofsY = (int32_t)(note.startOfsY * (1.0f - frac));

        // The timing arrow turns once over the note's flight
        uint32_t turn = note.flyTime ? (uint32_t)(((uint64_t)elapsed * 65536) / note.flyTime) : 0;
        note.arrow = (uint16_t)(0u - turn);
    }
}

void GameEngine::beginFrame(uint32_t now, uint32_t dt)
{
    currentTime = now;
    frameDelta = dt;

    // Execute chart events
    while (eventIndex < chart.events.size() && chart.events[eventIndex].time <= now)
    {
        const ChartEvent &event = chart.events[eventIndex++];

        switch (event.type)
        {
            case ChartEventType::Target:
            {
                createNote(event);
                break;
            }

            case ChartEventType::BarTimeSet:
            {
                // The chart's tempo gives the flight time of the notes that
                // follow: one bar of (beatsPerBar + 1) beats, in 10 us units.
                flyTime = (uint32_t)((60.0f / event.bpm) * (event.beatsPerBar + 1) * 100000);
                break;
            }

            case ChartEventType::TargetFlyingTime:
            {
                // Set the time between a note's creation and when it should be hit
                flyTime = event.flyingTimeMs * 100;
                break;
            }

            case ChartEventType::Lyric:
            {
                lyricIndex = event.lyricIndex;
                hasLyric = true;
                break;
            }

            case ChartEventType::MusicPlay:
            {
                musicStarted = true;
                break;
            }

            default:
            {
                break;
            }
        }
    }

    if (chart.ended && eventIndex >= chart.events.size() && now >= chart.endTime)
        finished = true;

    // The judgement is shown for a while
    if (statTimer > 0)
        statTimer--;

    updateNotePositions();
}

bool GameEngine::judgeNotes(ActionMask down, ActionMask held, ActionMask up)
{
    // The comparison is signed: in unsigned arithmetic a note that comes
    // before the miss window would wrap around, and the notes at the very
    // start of a chart would never be judged
    if (notes.empty() || (int32_t)(notes[0].time - currentTime) >= WINDOW_MISS)
        return false;

    if (!mask)
    {
        // Get the keys that need to be pressed for the current notes
        statX = statY = 0;
        while (current < notes.size() && notes[current].time == notes[0].time)
        {
            statX += notes[current].x + 16;
            statY += notes[current].y + 16;
            mask |= (uint8_t)(1u << (notes[current].type & NOTE_TYPE_MASK));
            current++;
        }
        statX = statX / current - 16;
        statY = statY / current - 28;
    }

    if (!mask)
        return false;

    // Scan key input and track which keys are pressed
    for (int i = 0; i < ActionCount; i++)
    {
        uint32_t bit = 1u << i;

        // Keep scanning after a wrong key was pressed, so a second wrong
        // button can register a miss against the next group of notes. The
        // queue is empty here only when the last group was just cleared, and
        // reading past the end of it would be undefined, so the scan stops.
        if (notes.empty())
            break;

        if ((notes[0].type & NOTE_SLIDE_CONT) && (held & bit))
        {
            if (mask & bit)
                mask2 |= (uint8_t)bit;
        }
        else if (!(notes[0].type & NOTE_SLIDE_CONT) && (down & bit))
        {
            if (mask & bit)
            {
                mask2 |= (uint8_t)bit;
                continue;
            }

            // Miss if a wrong key is pressed
            statType = JudgementMiss;
            statTimer = 60;
            statCurX = statX;
            statCurY = statY;
            results.misses++;
            combo = 0;

            // Check how precisely the note was hit and adjust life and score
            // For slides, fine/safe count as cool, and sad counts as fine
            int32_t offset = abs((int32_t)(notes[0].time - currentTime));
            bool slide = (notes[0].type & (NOTE_SLIDE | NOTE_SLIDE_CHAIN | NOTE_SLIDE_CONT)) != 0;

            if (offset <= (slide ? 9 : 3) * (int32_t)NOMINAL_FRAME_TIME) // Wrong (red)
            {
                life = (uint8_t)std::max(0, (int)life - 3);
                results.scoreBase += 250;
            }
            else if (offset <= 6 * (int32_t)NOMINAL_FRAME_TIME || slide) // Wrong (black)
            {
                life = (uint8_t)std::max(0, (int)life - 6);
                results.scoreBase += 150;
            }
            else if (offset <= 9 * (int32_t)NOMINAL_FRAME_TIME) // Wrong (green)
            {
                life = (uint8_t)std::max(0, (int)life - 9);
                results.scoreBase += 50;
            }
            else // Wrong (blue)
            {
                life = (uint8_t)std::max(0, (int)life - 15);
                results.scoreBase += 30;
            }

            // Clear notes that were missed
            for (; current > 0; current--)
                notes.pop_front();
            mask = mask2 = 0;
        }
        else if (up & bit)
        {
            mask2 &= ~(uint8_t)bit;
        }
    }

    // A frame with a wrong key ends here; the next frame sees the key as
    // already held rather than newly pressed
    if (!mask)
        return true;

    if (mask == mask2)
    {
        if (notes[0].type & NOTE_SLIDE_CONT)
        {
            // Adjust score for non-initial held slides, which are always cool
            // A score bonus is added based on the current "combo" of these notes
            results.scoreBase += 500;
            results.scoreSlide += (++slideCount) * 10;

            // Detect the end of a held slide
            if (notes.size() == current || !(notes[current].type & NOTE_SLIDE_CONT))
            {
                // Add a 1000-point bonus if the slide was never broken
                if (!slideBroken)
                    results.scoreSlide += 1000;

                // Reset the slide stats for the next one
                slideCount = 0;
                slideBroken = false;
            }

            // Add a 10-point bonus at full health
            if (life == 255)
                results.scoreBase += 10;
        }
        else
        {
            // Check how precisely the note was hit and adjust life and score
            // For slides, fine/safe count as cool, and sad counts as fine
            int32_t offset = abs((int32_t)(notes[0].time - currentTime));
            bool slide = (notes[0].type & (NOTE_SLIDE | NOTE_SLIDE_CHAIN | NOTE_SLIDE_CONT)) != 0;

            if (offset <= (slide ? 9 : 3) * (int32_t)NOMINAL_FRAME_TIME) // Cool
            {
                statType = JudgementCool;
                combo++;
                life = (uint8_t)std::min(255, (int)life + 2);
                results.scoreBase += 500 * current;
                results.cools++;

                // Add a 10-point bonus at full health
                if (life == 255)
                    results.scoreBase += 10;
            }
            else if (offset <= 6 * (int32_t)NOMINAL_FRAME_TIME || slide) // Fine
            {
                statType = JudgementFine;
                combo++;
                life = (uint8_t)std::min(255, (int)life + 1);
                results.scoreBase += 300 * current;
                results.fines++;
            }
            else if (offset <= 9 * (int32_t)NOMINAL_FRAME_TIME) // Safe
            {
                statType = JudgementSafe;
                combo = 0;
                results.scoreBase += 100 * current;
                results.safes++;
            }
            else // Sad
            {
                statType = JudgementSad;
                combo = 0;
                life = (uint8_t)std::max(0, (int)life - 10);
                results.scoreBase += 50 * current;
                results.sads++;
            }

            // Show the hit status above the note
            statTimer = 60;
            statCurX = statX;
            statCurY = statY;

            // Add a score bonus based on current combo
            if (combo >= 50)
                results.scoreBase += 250;
            else if (combo >= 40)
                results.scoreBase += 200;
            else if (combo >= 30)
                results.scoreBase += 150;
            else if (combo >= 20)
                results.scoreBase += 100;
            else if (combo >= 10)
                results.scoreBase += 50;

            for (uint32_t i = 0; i < current; i++)
            {
                // Cancel note holds if a held note is hit again
                if (holdNotes & (1u << (notes[i].type & NOTE_TYPE_MASK)))
                {
                    holdNotes = 0;
                    holdStart = 0;
                    holdTime = 0;
                    holdScore = 0;
                    holdFrameAccum = 0;
                    holdTimeAccum = 0;
                }

                // Track note holds and reset the max hold time when a new one starts
                if (notes[i].type & NOTE_HELD)
                {
                    holdNotes |= (uint8_t)(1u << (notes[i].type & NOTE_TYPE_MASK));
                    holdTime = 0;
                    holdTimeAccum = 0;
                }
            }
        }

        // Clear the notes that were hit
        for (; current > 0; current--)
            notes.pop_front();
        mask = mask2 = 0;
    }
    else if (notes[0].time + WINDOW_MISS < currentTime)
    {
        if (notes[0].type & NOTE_SLIDE_CONT)
        {
            // Break the slide combo if a held slide wasn't cleared in time
            slideCount = 0;
            slideBroken = true;
        }
        else
        {
            // Miss if a note wasn't cleared in time
            statType = JudgementMiss;
            statTimer = 60;
            statCurX = statX;
            statCurY = statY;
            combo = 0;
            life = (uint8_t)std::max(0, (int)life - 20);
            results.misses++;
        }

        // Clear the notes that were missed
        for (; current > 0; current--)
            notes.pop_front();
        mask = mask2 = 0;
    }

    return false;
}

void GameEngine::updateHoldBonus(ActionMask held)
{
    // Cancel note holds if one was released
    for (int i = 0; i < 4; i++)
    {
        if ((holdNotes & (1u << i)) && !(held & (1u << i)))
        {
            holdNotes = 0;
            holdStart = 0;
            holdTime = 0;
            holdScore = 0;
            holdFrameAccum = 0;
            holdTimeAccum = 0;
        }
    }

    if (!holdNotes)
        return;

    // The hold bonus is paid out once per nominal frame of elapsed time, not
    // once per rendered frame, so it does not depend on the frame rate.
    holdFrameAccum += frameDelta;

    while (holdFrameAccum >= NOMINAL_FRAME_TIME)
    {
        holdFrameAccum -= NOMINAL_FRAME_TIME;

        if (holdStart < 12)
        {
            // Queue a 10-point bonus per note hold
            for (int i = 0; i < 4; i++)
                if (holdNotes & (1u << i))
                    holdScore += 10;

            // After 12 frames, commit to the hold and add the queued bonus
            holdStart++;
            if (holdStart == 12)
                results.scoreHold += holdScore;
        }
        else
        {
            // Add a 10-point bonus per note hold
            for (int i = 0; i < 4; i++)
                if (holdNotes & (1u << i))
                    results.scoreHold += 10;
        }
    }

    // A hold ends after five seconds
    holdTimeAccum += frameDelta;

    while (holdTimeAccum >= NOMINAL_FRAME_TIME)
    {
        holdTimeAccum -= NOMINAL_FRAME_TIME;
        holdTime++;

        if (holdTime == 5 * 60)
        {
            // Add a 1500-point bonus per note hold if the max hold time is reached
            for (int i = 0; i < 4; i++)
                if (holdNotes & (1u << i))
                    results.scoreHold += 1500;

            // Cancel note holds after the max hold time is reached
            holdNotes = 0;
            holdStart = 0;
            holdTime = 0;
            holdScore = 0;
            holdFrameAccum = 0;
            holdTimeAccum = 0;
            break;
        }
    }
}

void GameEngine::handleInput(ActionMask down, ActionMask held, ActionMask up)
{
    // Re-run a frame after a wrong key was pressed, with the key
    // no longer counted as newly pressed, so that a
    // held slide is not judged a frame earlier than it should be.
    ActionMask frameDown = down;

    for (int pass = 0; pass < 2; pass++)
    {
        if (!judgeNotes(frameDown, held, up))
            break;

        frameDown = 0;
    }

    updateHoldBonus(held);

    // Update the maximum combo result
    if (combo > results.comboMax)
        results.comboMax = combo;

    // Calculate the current clear percentage, with up to 5% bonus from holds
    if (scoreRef)
    {
        uint32_t holdBonus = std::min(scoreRef / 20, (results.scoreHold * 4) / holdDivide);
        results.clear = (100.0f * (results.scoreBase + holdBonus)) / scoreRef;
    }
}

bool GameEngine::isMultiNote(const std::deque<Note> &notes, size_t index)
{
    // Check if this is a multi-note so variant graphics can be used
    return (index > 0 && notes[index - 1].time == notes[index].time) ||
           (index + 1 < notes.size() && notes[index + 1].time == notes[index].time);
}

} // namespace p3ds
