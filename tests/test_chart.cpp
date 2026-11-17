/*
    Copyright 2026 Project 3DS contributors

    SPDX-License-Identifier: GPL-3.0-or-later
    This file is licensed under the GNU General Public License, version 3 or
    later. It is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See LICENSE for the full licence text.
*/

#include "test_framework.h"

#include "core/chart.h"
#include "scenario.h"

using namespace p3ds;

namespace {

std::vector<uint8_t> bytesFromWords(const std::vector<uint32_t> &words)
{
    std::vector<uint8_t> bytes(words.size() * 4);

    for (size_t i = 0; i < words.size(); i++)
    {
        bytes[i * 4 + 0] = (uint8_t)(words[i] & 0xFF);
        bytes[i * 4 + 1] = (uint8_t)((words[i] >> 8) & 0xFF);
        bytes[i * 4 + 2] = (uint8_t)((words[i] >> 16) & 0xFF);
        bytes[i * 4 + 3] = (uint8_t)((words[i] >> 24) & 0xFF);
    }

    return bytes;
}

size_t countEvents(const Chart &chart, ChartEventType type)
{
    size_t count = 0;

    for (size_t i = 0; i < chart.events.size(); i++)
    {
        if (chart.events[i].type == type)
            count++;
    }

    return count;
}

} // namespace

TEST(chart_parses_every_opcode_the_build_uses)
{
    scenario::ChartBuilder builder;
    builder.time(1000);
    builder.targetFlyingTime(500);
    builder.barTimeSet(120, 3);
    builder.lyric(7);
    builder.musicPlay();
    builder.time(2000);
    builder.target(0, 240000, 135000, 45000, 100000);
    builder.end();

    Chart chart = ChartLoader::parse(builder.bytes().data(), builder.bytes().size());

    CHECK(chart.valid);
    CHECK(chart.ended);
    CHECK_EQ(countEvents(chart, ChartEventType::Target), 1u);
    CHECK_EQ(countEvents(chart, ChartEventType::Lyric), 1u);
    CHECK_EQ(countEvents(chart, ChartEventType::MusicPlay), 1u);
    CHECK_EQ(countEvents(chart, ChartEventType::BarTimeSet), 1u);
    CHECK_EQ(countEvents(chart, ChartEventType::TargetFlyingTime), 1u);

    // The target is executed at the time of the last time opcode
    for (size_t i = 0; i < chart.events.size(); i++)
    {
        if (chart.events[i].type == ChartEventType::Target)
            CHECK_EQ(chart.events[i].time, 2000u);
    }
}

TEST(chart_skips_the_opcodes_the_build_does_not_use)
{
    // A chart full of opcodes the parser does not consume still parses
    std::vector<uint32_t> words;
    words.push_back(0);          // the header
    words.push_back(0x02);       // MIKU_MOVE, four parameters
    words.push_back(1);
    words.push_back(2);
    words.push_back(3);
    words.push_back(4);
    words.push_back(0x09);       // EFFECT, six parameters
    for (int i = 0; i < 6; i++)
        words.push_back(i);
    words.push_back(0x6A);       // PSE, two parameters
    words.push_back(0);
    words.push_back(0);
    words.push_back(0x00);       // END

    std::vector<uint8_t> bytes(words.size() * 4);

    for (size_t i = 0; i < words.size(); i++)
    {
        bytes[i * 4 + 0] = (uint8_t)(words[i] & 0xFF);
        bytes[i * 4 + 1] = (uint8_t)((words[i] >> 8) & 0xFF);
        bytes[i * 4 + 2] = (uint8_t)((words[i] >> 16) & 0xFF);
        bytes[i * 4 + 3] = (uint8_t)((words[i] >> 24) & 0xFF);
    }

    Chart chart = ChartLoader::parse(bytes.data(), bytes.size());

    CHECK(chart.valid);
    CHECK_EQ(chart.events.size(), 0u);
}

TEST(chart_skips_opcodes_above_the_known_table)
{
    // The opcode table has 0x100 entries; opcodes above 0x6A have no
    // parameters and are skipped, so a known END after them is still reached
    std::vector<uint32_t> words;
    words.push_back(0);    // the header
    words.push_back(0x6B); // unknown, no parameters
    words.push_back(0xFE); // unknown, no parameters
    words.push_back(0x00); // END

    std::vector<uint8_t> bytes = bytesFromWords(words);
    Chart chart = ChartLoader::parse(bytes.data(), bytes.size());

    CHECK(chart.valid);
    CHECK(chart.ended);
    CHECK_EQ(chart.events.size(), 0u);
}

TEST(chart_classifies_the_note_types)
{
    scenario::ChartBuilder builder;
    builder.targetFlyingTime(500);

    // The note types the parser produces
    for (int i = 0; i < 8; i++)
        builder.target((uint8_t)i, 240000, 135000, 0, 100000);

    builder.time(1000);
    builder.target(12, 240000, 135000, 0, 100000);
    builder.target(13, 240000, 135000, 0, 100000);
    builder.target(18, 240000, 135000, 0, 100000);
    builder.target(21, 240000, 135000, 0, 100000);
    builder.end();

    Chart chart = ChartLoader::parse(builder.bytes().data(), builder.bytes().size());
    std::vector<uint8_t> types;

    for (size_t i = 0; i < chart.events.size(); i++)
    {
        if (chart.events[i].type == ChartEventType::Target)
            types.push_back(chart.events[i].target.noteType);
    }

    CHECK_EQ(types.size(), 12u);

    // Normal buttons keep their type
    for (int i = 0; i < 4; i++)
        CHECK_EQ(types[i], (uint8_t)i);

    // Held buttons are marked with bit 4
    for (int i = 4; i < 8; i++)
        CHECK_EQ(types[i], (uint8_t)((i & 3) | NOTE_HELD));

    // Single slides are marked with bit 5
    CHECK_EQ(types[8], (uint8_t)(4 | NOTE_SLIDE));
    CHECK_EQ(types[9], (uint8_t)(5 | NOTE_SLIDE));

    // Event notes are treated as regular notes
    CHECK_EQ(types[10], (uint8_t)0);
    CHECK_EQ(types[11], (uint8_t)3);
}

TEST(chart_ignores_the_note_types_the_build_ignores)
{
    scenario::ChartBuilder builder;
    builder.targetFlyingTime(500);

    for (int i = 8; i <= 11; i++)
        builder.target((uint8_t)i, 240000, 135000, 0, 100000);

    builder.target(17, 240000, 135000, 0, 100000);
    builder.end();

    Chart chart = ChartLoader::parse(builder.bytes().data(), builder.bytes().size());
    CHECK_EQ(countEvents(chart, ChartEventType::Target), 0u);
}

TEST(chart_marks_the_continuation_of_a_held_slide)
{
    scenario::ChartBuilder builder;
    builder.targetFlyingTime(500);

    // A held slide that moves to the left continues the previous one
    builder.target(15, 240000, 135000, 0, 100000);
    builder.target(15, 200000, 135000, 0, 100000);
    builder.target(15, 160000, 135000, 0, 100000);

    // A held slide that moves to the right continues the previous one
    builder.target(16, 240000, 135000, 0, 100000);
    builder.target(16, 280000, 135000, 0, 100000);

    // A held slide at another height starts a new one
    builder.target(15, 240000, 100000, 0, 100000);
    builder.end();

    Chart chart = ChartLoader::parse(builder.bytes().data(), builder.bytes().size());
    std::vector<uint8_t> types;

    for (size_t i = 0; i < chart.events.size(); i++)
    {
        if (chart.events[i].type == ChartEventType::Target)
            types.push_back(chart.events[i].target.noteType);
    }

    CHECK_EQ(types.size(), 6u);
    CHECK_EQ(types[0], (uint8_t)(4 | NOTE_SLIDE_CHAIN));
    CHECK_EQ(types[1], (uint8_t)(4 | NOTE_SLIDE_CHAIN | NOTE_SLIDE_CONT));
    CHECK_EQ(types[2], (uint8_t)(4 | NOTE_SLIDE_CHAIN | NOTE_SLIDE_CONT));
    CHECK_EQ(types[3], (uint8_t)(5 | NOTE_SLIDE_CHAIN));
    CHECK_EQ(types[4], (uint8_t)(5 | NOTE_SLIDE_CHAIN | NOTE_SLIDE_CONT));
    CHECK_EQ(types[5], (uint8_t)(4 | NOTE_SLIDE_CHAIN));
}

TEST(chart_marks_the_continuation_of_a_right_held_slide)
{
    scenario::ChartBuilder builder;
    builder.targetFlyingTime(500);

    // A right held slide moves to the right, and each step continues the
    // previous one
    builder.target(16, 200000, 135000, 0, 100000);
    builder.target(16, 240000, 135000, 0, 100000);
    builder.target(16, 280000, 135000, 0, 100000);

    // Moving the other way starts a new chain
    builder.target(16, 240000, 135000, 0, 100000);
    builder.end();

    Chart chart = ChartLoader::parse(builder.bytes().data(), builder.bytes().size());
    std::vector<uint8_t> types;

    for (size_t i = 0; i < chart.events.size(); i++)
    {
        if (chart.events[i].type == ChartEventType::Target)
            types.push_back(chart.events[i].target.noteType);
    }

    CHECK_EQ(types.size(), 4u);
    CHECK_EQ(types[0], (uint8_t)(5 | NOTE_SLIDE_CHAIN));
    CHECK_EQ(types[1], (uint8_t)(5 | NOTE_SLIDE_CHAIN | NOTE_SLIDE_CONT));
    CHECK_EQ(types[2], (uint8_t)(5 | NOTE_SLIDE_CHAIN | NOTE_SLIDE_CONT));
    CHECK_EQ(types[3], (uint8_t)(5 | NOTE_SLIDE_CHAIN));
}

TEST(chart_groups_the_notes_that_share_a_time)
{
    scenario::ChartBuilder builder;
    builder.targetFlyingTime(500);

    // Two notes without a time opcode between them are one group
    builder.target(0, 240000, 135000, 0, 100000);
    builder.target(1, 240000, 135000, 0, 100000);

    // A time opcode starts a new group
    builder.time(1000);
    builder.target(2, 240000, 135000, 0, 100000);
    builder.end();

    Chart chart = ChartLoader::parse(builder.bytes().data(), builder.bytes().size());
    std::vector<uint32_t> groups;

    for (size_t i = 0; i < chart.events.size(); i++)
    {
        if (chart.events[i].type == ChartEventType::Target)
            groups.push_back(chart.events[i].target.group);
    }

    CHECK_EQ(groups.size(), 3u);
    CHECK_EQ(groups[0], groups[1]);
    CHECK_NE(groups[1], groups[2]);
}

TEST(chart_rejects_a_truncated_chart)
{
    // A time opcode without its parameter
    std::vector<uint8_t> bytes(8, 0);
    bytes[4] = 0x01;
    Chart chart = ChartLoader::parse(bytes.data(), bytes.size());
    CHECK(!chart.valid);

    Chart empty = ChartLoader::parse(nullptr, 0);
    CHECK(!empty.valid);
}
