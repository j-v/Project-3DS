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

#ifndef P3DS_DATABASE_H
#define P3DS_DATABASE_H

#include <string>
#include <vector>

#include "results.h"

namespace p3ds {

/** The number of difficulties. */
constexpr int DifficultyCount = 5;

/** The number of songs the database can hold. */
constexpr int MaxSongs = 1000;

/** Everything the game knows about one song. */
struct SongData
{
    std::string name;
    std::vector<std::string> lyrics;

    // The difficulty levels, in half units (12.5 is stored as 25)
    uint32_t difficulties[DifficultyCount] = {};

    uint32_t scores[DifficultyCount] = {};
    float clears[DifficultyCount] = {};
    uint8_t ranks[DifficultyCount] = {};
};

/**
 * The song database and the saved scores.
 *
 * The core never touches the file system: the platform reads the files and
 * hands the text over, and the platform writes back what serializeScores
 * returns.
 */
class SongDatabase
{
public:
    SongDatabase();

    /** Parses the contents of one _db.txt file. */
    void parseDatabaseText(const std::string &text);

    /** Parses the contents of a scores.txt file. */
    void parseScoresText(const std::string &text);

    /** Builds the contents of a scores.txt file. */
    std::string serializeScores() const;

    /** Adds a chart file name to the song list, if it is one. */
    void addChartFileName(const std::string &name);

    /**
     * Sorts the song list of every difficulty.
     *
     * @param alphabetical  sort by name instead of by difficulty level
     */
    void sortCharts(bool alphabetical);

    /** The song IDs of a difficulty, in list order. */
    const std::vector<std::string> &charts(int difficulty) const { return chartList[difficulty]; }

    SongData &song(int id) { return songs[id]; }
    const SongData &song(int id) const { return songs[id]; }

    /** The tag scores.txt uses for a difficulty. */
    static const char *difficultyTag(int difficulty);

    /** The name of a difficulty, as the results screen shows it. */
    static const char *difficultyName(int difficulty);

    /** The name of a rank, as the song list shows it. */
    static const char *rankName(int rank);

    /** The clear percentage needed for each rank, per difficulty. */
    static uint8_t rankThreshold(int difficulty, int rank);

    /** The rank a result earns. */
    static uint8_t calculateRank(const Results &results, int difficulty, bool fail);

private:
    SongData songs[MaxSongs];
    std::vector<std::string> chartList[DifficultyCount];
};

} // namespace p3ds

#endif // P3DS_DATABASE_H
