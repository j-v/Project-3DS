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

#include "song_database.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace p3ds {

/** The file name suffixes of each difficulty. */
static const char *chartEnds[DifficultyCount] =
{
    "_easy.dsc",
    "_normal.dsc",
    "_hard.dsc",
    "_extreme.dsc",
    "_extreme_1.dsc"
};

/** The tags scores.txt uses for each difficulty. */
static const char *difficultyTags[DifficultyCount] =
{
    "easy.0.",
    "normal.0.",
    "hard.0.",
    "extreme.0.",
    "extreme.1."
};

/** The names of the difficulties, as the results screen shows them. */
static const char *difficultyNames[DifficultyCount] =
{
    "EASY",
    "NORMAL",
    "HARD",
    "EXTREME",
    "EX EXTREME"
};

/** The names of the ranks, as the song list shows them. */
static const char *rankNames[5] = { "", "(S)", "(G)", "(E)", "(P)" };

/** The clear percentage needed for each rank, per difficulty. */
static const uint8_t rankThresholds[DifficultyCount][3] =
{
    { 30, 65, 80 }, // Easy
    { 50, 75, 85 }, // Normal
    { 60, 80, 90 }, // Hard
    { 70, 85, 95 }, // Extreme
    { 70, 85, 95 }  // Extra Extreme
};

/**
 * The character priorities used to sort song names.
 *
 * A plain std::string comparison would put uppercase before lowercase, digits
 * before letters, and punctuation first. The table instead folds case (A and a
 * share a priority), puts letters before digits (2-27 against 28-37), gives
 * punctuation priority 0 so leading punctuation is skipped, and buckets every
 * non-ASCII byte under 36 so names with such characters still sort
 * deterministically.
 */
static const uint8_t namePriorities[256] =
{
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 0x00-0x0F
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 0x10-0x1F
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 0x20-0x2F
    28, 29, 30, 31, 32, 33, 34, 35, 36, 37,  0,  0,  0,  0,  0,  0, // 0x30-0x3F
     0,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, // 0x40-0x4F
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,  0,  0,  0,  0,  0, // 0x50-0x5F
     0,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, // 0x60-0x6F
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,  0,  0,  0,  0,  0, // 0x70-0x7F
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, // 0x80-0x8F
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, // 0x90-0x9F
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, // 0xA0-0xAF
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, // 0xB0-0xBF
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, // 0xC0-0xCF
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, // 0xD0-0xDF
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, // 0xE0-0xEF
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36  // 0xF0-0xFF
};

/** Removes the line ending and any repeated spaces. */
static std::string tidyString(std::string str)
{
    size_t end = str.size();

    while (end > 0 && (str[end - 1] == '\n' || str[end - 1] == '\r'))
        str.erase(--end, 1);

    // Remove consecutive spaces
    for (size_t i = 1; i < str.size();)
    {
        if (str[i] == ' ' && str[i - 1] == ' ')
            str.erase(i, 1);
        else
            i++;
    }

    return str;
}

/** Parses a level like "PV_LV_12_5" into half units. */
static bool parseLevel(const std::string &value, uint32_t &level)
{
    // The whole and fractional digits are read from fixed positions
    if (value.length() < 10 ||
        value[6] < '0' || value[6] > '9' ||
        value[7] < '0' || value[7] > '9' ||
        value[9] < '0' || value[9] > '9')
        return false;

    uint32_t intPart = (uint32_t)(value[6] - '0') * 10 + (uint32_t)(value[7] - '0');
    uint32_t fracDigit = (uint32_t)(value[9] - '0');
    level = intPart * 2 + fracDigit / 5;
    return true;
}

/** Compares two song names using the priority table. */
static bool nameLess(const std::string &nameA, const std::string &nameB)
{
    size_t j = 0, k = 0;

    // Ignore leading characters that aren't letters or numbers in the first string
    while (j < nameA.length() && !namePriorities[(uint8_t)nameA[j]])
        j++;

    // Ignore leading characters that aren't letters or numbers in the second string
    while (k < nameB.length() && !namePriorities[(uint8_t)nameB[k]])
        k++;

    while (j < nameA.length() && k < nameB.length())
    {
        // Sort the songs based on the first non-matching character priority
        uint8_t priorityA = namePriorities[(uint8_t)nameA[j]];
        uint8_t priorityB = namePriorities[(uint8_t)nameB[k]];

        if (priorityA != priorityB)
            return priorityA < priorityB;

        j++;
        k++;
    }

    // A name that runs out first sorts first, and equal names are not less
    return j == nameA.length() && k != nameB.length();
}

SongDatabase::SongDatabase()
{
    // Assign default song names as fallbacks
    for (int i = 0; i < MaxSongs; i++)
    {
        char name[16];
        snprintf(name, sizeof(name), "pv_%03d", i);
        songs[i].name = name;
    }
}

const char *SongDatabase::difficultyTag(int difficulty)
{
    return difficultyTags[difficulty < 0 || difficulty >= DifficultyCount ? 0 : difficulty];
}

const char *SongDatabase::difficultyName(int difficulty)
{
    return difficultyNames[difficulty < 0 || difficulty >= DifficultyCount ? 0 : difficulty];
}

const char *SongDatabase::rankName(int rank)
{
    return rankNames[rank < 0 || rank > 4 ? 0 : rank];
}

uint8_t SongDatabase::rankThreshold(int difficulty, int rank)
{
    if (difficulty < 0 || difficulty >= DifficultyCount)
        difficulty = 0;
    if (rank < 0 || rank > 2)
        rank = 0;
    return rankThresholds[difficulty][rank];
}

uint8_t SongDatabase::calculateRank(const Results &results, int difficulty, bool fail)
{
    if (results.comboMax == results.total)
        return 4; // PERFECT

    if (fail || results.clear < rankThreshold(difficulty, 0))
        return 0; // DROP

    if (results.clear < rankThreshold(difficulty, 1))
        return 1; // STANDARD

    if (results.clear < rankThreshold(difficulty, 2))
        return 2; // GREAT

    return 3; // EXCELLENT
}

void SongDatabase::parseDatabaseText(const std::string &text)
{
    size_t pos = 0;

    while (pos < text.size())
    {
        size_t end = text.find('\n', pos);
        std::string line = (end == std::string::npos) ? text.substr(pos) : text.substr(pos, end - pos);
        pos = (end == std::string::npos) ? text.size() : end + 1;

        // Every line starts with the song's ID, followed by the field
        if (line.length() < 8 || line.substr(0, 3) != "pv_" || line[6] != '.')
            continue;

        if (line[3] < '0' || line[3] > '9' || line[4] < '0' || line[4] > '9' ||
            line[5] < '0' || line[5] > '9')
            continue;

        int id = (line[3] - '0') * 100 + (line[4] - '0') * 10 + (line[5] - '0');

        if (id < 0 || id >= MaxSongs)
            continue;

        std::string key = line.substr(7);

        if (key.compare(0, 13, "song_name_en=") == 0)
        {
            // Set a song name from the database
            songs[id].name = tidyString(key.substr(13));
        }
        else if (key.compare(0, 9, "lyric_en.") == 0)
        {
            // Set a song lyric from the database
            if (key.length() < 13 || key[12] != '=')
                continue;

            int index = 0;
            for (int i = 9; i < 12; i++)
            {
                if (key[i] < '0' || key[i] > '9')
                {
                    index = -1;
                    break;
                }
                index = index * 10 + (key[i] - '0');
            }

            if (index < 0)
                continue;

            std::vector<std::string> &lyrics = songs[id].lyrics;

            if ((size_t)index >= lyrics.size())
                lyrics.resize(index + 1);

            lyrics[index] = tidyString(key.substr(13));
        }
        else if (key.compare(0, 11, "difficulty.") == 0)
        {
            // Set a difficulty level as fixed-point with a 1-bit fractional
            static const char *modes[DifficultyCount] = { "easy", "normal", "hard", "extreme", "extreme" };
            static const int indices[DifficultyCount] = { 0, 0, 0, 0, 1 };

            size_t dot = key.find('.', 11);

            if (dot == std::string::npos)
                continue;

            std::string mode = key.substr(11, dot - 11);
            std::string rest = key.substr(dot + 1);

            if (rest.compare(0, 7, "0.level") != 0 && rest.compare(0, 7, "1.level") != 0)
                continue;

            int difficulty = -1;

            for (int i = 0; i < DifficultyCount; i++)
            {
                if (mode == modes[i] && rest[0] == '0' + indices[i])
                {
                    difficulty = i;
                    break;
                }
            }

            if (difficulty < 0)
                continue;

            size_t equals = key.find('=', 11);

            if (equals == std::string::npos)
                continue;

            uint32_t level = 0;

            if (!parseLevel(key.substr(equals + 1), level))
                continue;

            SongData &song = songs[id];

            // A zero level in another database file must not erase a real one
            if (level != 0 || song.difficulties[difficulty] == 0)
                song.difficulties[difficulty] = level;
        }
    }
}

void SongDatabase::parseScoresText(const std::string &text)
{
    size_t pos = 0;

    while (pos < text.size())
    {
        size_t end = text.find('\n', pos);
        std::string line = (end == std::string::npos) ? text.substr(pos) : text.substr(pos, end - pos);
        pos = (end == std::string::npos) ? text.size() : end + 1;

        if (line.length() < 8 || line.substr(0, 3) != "pv_" || line[6] != '.')
            continue;

        if (line[3] < '0' || line[3] > '9' || line[4] < '0' || line[4] > '9' ||
            line[5] < '0' || line[5] > '9')
            continue;

        int id = (line[3] - '0') * 100 + (line[4] - '0') * 10 + (line[5] - '0');

        if (id < 0 || id >= MaxSongs)
            continue;

        // The field name is between the last dot and the equals sign
        size_t equals = line.find('=', 7);

        if (equals == std::string::npos)
            continue;

        size_t dot = line.rfind('.', equals);

        if (dot == std::string::npos || dot < 7)
            continue;

        std::string tag = line.substr(7, dot - 7 + 1);
        std::string field = line.substr(dot + 1, equals - dot - 1);
        std::string value = line.substr(equals + 1);

        int difficulty = -1;

        for (int i = 0; i < DifficultyCount; i++)
        {
            if (tag == difficultyTags[i])
            {
                difficulty = i;
                break;
            }
        }

        if (difficulty < 0)
            continue;

        if (field == "score")
            songs[id].scores[difficulty] = (uint32_t)strtoul(value.c_str(), nullptr, 10);
        else if (field == "clear")
            songs[id].clears[difficulty] = (float)strtod(value.c_str(), nullptr);
        else if (field == "rank")
            songs[id].ranks[difficulty] = (uint8_t)strtoul(value.c_str(), nullptr, 10);
    }
}

std::string SongDatabase::serializeScores() const
{
    std::string out;

    for (int i = 0; i < MaxSongs; i++)
    {
        for (int diff = 0; diff < DifficultyCount; diff++)
        {
            char line[64];

            // Add an entry for each non-zero score value
            if (songs[i].scores[diff])
            {
                snprintf(line, sizeof(line), "pv_%03d.%sscore=%lu\n", i, difficultyTags[diff],
                         (unsigned long)songs[i].scores[diff]);
                out += line;
            }

            if (songs[i].clears[diff])
            {
                snprintf(line, sizeof(line), "pv_%03d.%sclear=%.02f\n", i, difficultyTags[diff],
                         (double)songs[i].clears[diff]);
                out += line;
            }

            if (songs[i].ranks[diff])
            {
                snprintf(line, sizeof(line), "pv_%03d.%srank=%u\n", i, difficultyTags[diff],
                         (unsigned int)songs[i].ranks[diff]);
                out += line;
            }
        }
    }

    return out;
}

void SongDatabase::addChartFileName(const std::string &name)
{
    if (name.length() <= 6 || name.substr(0, 3) != "pv_")
        return;

    if (name[3] < '0' || name[3] > '9' || name[4] < '0' || name[4] > '9' ||
        name[5] < '0' || name[5] > '9')
        return;

    for (int i = 0; i < DifficultyCount; i++)
    {
        if (name.substr(6) == chartEnds[i])
        {
            chartList[i].push_back(name.substr(3, 3));
            break;
        }
    }
}

void SongDatabase::sortCharts(bool alphabetical)
{
    // Sort the songs in each difficulty tab
    for (int i = 0; i < DifficultyCount; i++)
    {
        std::stable_sort(chartList[i].begin(), chartList[i].end(),
                         [this, i, alphabetical](const std::string &a, const std::string &b)
        {
            uint32_t diffA = songs[atoi(a.c_str())].difficulties[i];
            uint32_t diffB = songs[atoi(b.c_str())].difficulties[i];

            // Sort alphabetically in alphabetical mode, or as a fallback if difficulties match
            if (alphabetical || diffA == diffB)
                return nameLess(songs[atoi(a.c_str())].name, songs[atoi(b.c_str())].name);

            // Sort the songs based on difficulty level
            return diffA < diffB;
        });
    }
}

} // namespace p3ds
