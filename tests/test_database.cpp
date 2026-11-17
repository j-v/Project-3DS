/*
    Copyright 2026 Project 3DS contributors

    SPDX-License-Identifier: GPL-3.0-or-later
    This file is licensed under the GNU General Public License, version 3 or
    later. It is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See LICENSE for the full licence text.
*/

#include "test_framework.h"

#include "core/song_database.h"

using namespace p3ds;

TEST(database_reads_the_song_names)
{
    SongDatabase database;
    database.parseDatabaseText(
        "pv_001.song_name_en=Sample Song\n"
        "pv_002.song_name_en=Another Song\n");

    CHECK_EQ(database.song(1).name, std::string("Sample Song"));
    CHECK_EQ(database.song(2).name, std::string("Another Song"));

    // The songs that are not in the database keep their fallback name
    CHECK_EQ(database.song(3).name, std::string("pv_003"));
}

TEST(database_keeps_the_japanese_characters)
{
    // Characters outside ASCII are kept, because the 3DS font can show them
    SongDatabase database;
    database.parseDatabaseText("pv_001.song_name_en=\xe3\x83\x9f\xe3\x83\x8b\xe3\x83\xa5\n");

    CHECK(database.song(1).name.size() > 0);
    CHECK((unsigned char)database.song(1).name[0] >= 0x80);
}

TEST(database_reads_the_lyrics)
{
    SongDatabase database;
    database.parseDatabaseText(
        "pv_001.lyric_en.000=First line\n"
        "pv_001.lyric_en.002=Third line\n");

    CHECK_EQ(database.song(1).lyrics.size(), 3u);
    CHECK_EQ(database.song(1).lyrics[0], std::string("First line"));
    CHECK_EQ(database.song(1).lyrics[1], std::string(""));
    CHECK_EQ(database.song(1).lyrics[2], std::string("Third line"));
}

TEST(database_reads_levels_in_the_ds_format)
{
    SongDatabase database;
    database.parseDatabaseText(
        "pv_001.difficulty.easy.0.level=PV_LV_02_0\n"
        "pv_001.difficulty.normal.0.level=pv_lv_05_5\n"
        "pv_001.difficulty.hard.0.level=PV_LV_07_5\n"
        "pv_001.difficulty.extreme.0.level=PV_LV_09_0\n"
        "pv_001.difficulty.extreme.1.level=PV_LV_10_5\n");

    CHECK_EQ(database.song(1).difficulties[0], 4u);
    CHECK_EQ(database.song(1).difficulties[1], 11u);
    CHECK_EQ(database.song(1).difficulties[2], 15u);
    CHECK_EQ(database.song(1).difficulties[3], 18u);
    CHECK_EQ(database.song(1).difficulties[4], 21u);
}

TEST(database_ignores_levels_it_cannot_parse)
{
    // Only the fixed-position PV_LV form is understood; a plain number is
    // ignored rather than parsed
    SongDatabase database;
    database.parseDatabaseText(
        "pv_001.difficulty.hard.0.level=12.5\n"
        "pv_002.difficulty.hard.0.level=PV_LV_05_5\n");

    CHECK_EQ(database.song(1).difficulties[2], 0u);
    CHECK_EQ(database.song(2).difficulties[2], 11u);
}

TEST(database_keeps_a_zero_level_from_erasing_a_real_one)
{
    // A zero entry in another database file must not erase a real level
    SongDatabase database;
    database.parseDatabaseText(
        "pv_271.difficulty.hard.0.level=PV_LV_05_5\n"
        "pv_271.difficulty.hard.0.level=PV_LV_00_0\n");

    CHECK_EQ(database.song(271).difficulties[2], 11u);
}

TEST(database_reads_the_saved_scores)
{
    SongDatabase database;
    database.parseScoresText(
        "pv_001.easy.0.score=123456\n"
        "pv_001.easy.0.clear=78.50\n"
        "pv_001.easy.0.rank=2\n"
        "pv_002.normal.0.score=654321\n"
        "pv_003.extreme.1.score=111222\n"
        "pv_004.extreme.0.clear=99.99\n"
        "pv_005.extreme.1.rank=4\n");

    CHECK_EQ(database.song(1).scores[0], 123456u);
    CHECK_NEAR(database.song(1).clears[0], 78.50, 0.001);
    CHECK_EQ(database.song(1).ranks[0], 2u);

    CHECK_EQ(database.song(2).scores[1], 654321u);

    CHECK_EQ(database.song(3).scores[4], 111222u);

    CHECK_NEAR(database.song(4).clears[3], 99.99, 0.001);

    CHECK_EQ(database.song(5).ranks[4], 4u);
}

TEST(database_writes_the_saved_scores_in_the_ds_format)
{
    SongDatabase database;
    database.song(1).scores[0] = 123456;
    database.song(1).clears[0] = 78.5f;
    database.song(1).ranks[0] = 2;
    database.song(2).scores[4] = 42;

    std::string text = database.serializeScores();

    CHECK(text.find("pv_001.easy.0.score=123456\n") != std::string::npos);
    CHECK(text.find("pv_001.easy.0.clear=78.50\n") != std::string::npos);
    CHECK(text.find("pv_001.easy.0.rank=2\n") != std::string::npos);
    CHECK(text.find("pv_002.extreme.1.score=42\n") != std::string::npos);

    // The scores that were written have to be read back the same way
    SongDatabase other;
    other.parseScoresText(text);
    CHECK_EQ(other.song(1).scores[0], 123456u);
    CHECK_NEAR(other.song(1).clears[0], 78.50, 0.001);
    CHECK_EQ(other.song(1).ranks[0], 2u);
    CHECK_EQ(other.song(2).scores[4], 42u);
}

TEST(database_collects_the_chart_files)
{
    SongDatabase database;
    database.addChartFileName("pv_001_easy.dsc");
    database.addChartFileName("pv_001_normal.dsc");
    database.addChartFileName("pv_002_extreme_1.dsc");
    database.addChartFileName("not_a_chart.txt");
    database.addChartFileName("pv_3_easy.dsc");

    CHECK_EQ(database.charts(0).size(), 1u);
    CHECK_EQ(database.charts(0)[0], std::string("001"));
    CHECK_EQ(database.charts(1).size(), 1u);
    CHECK_EQ(database.charts(4).size(), 1u);
    CHECK_EQ(database.charts(4)[0], std::string("002"));
}

TEST(database_sorts_the_songs_by_difficulty)
{
    SongDatabase database;
    database.addChartFileName("pv_001_easy.dsc");
    database.addChartFileName("pv_002_easy.dsc");
    database.addChartFileName("pv_003_easy.dsc");

    database.song(1).difficulties[0] = 30; // 15.0
    database.song(2).difficulties[0] = 10; // 5.0
    database.song(3).difficulties[0] = 20; // 10.0

    database.sortCharts(false);

    CHECK_EQ(database.charts(0)[0], std::string("002"));
    CHECK_EQ(database.charts(0)[1], std::string("003"));
    CHECK_EQ(database.charts(0)[2], std::string("001"));
}

TEST(database_sorts_the_songs_by_name)
{
    SongDatabase database;
    database.addChartFileName("pv_001_easy.dsc");
    database.addChartFileName("pv_002_easy.dsc");
    database.addChartFileName("pv_003_easy.dsc");

    database.song(1).name = "Zebra";
    database.song(2).name = "apple";
    database.song(3).name = "Banana";

    database.sortCharts(true);

    // The names are sorted without regard to their case
    CHECK_EQ(database.charts(0)[0], std::string("002"));
    CHECK_EQ(database.charts(0)[1], std::string("003"));
    CHECK_EQ(database.charts(0)[2], std::string("001"));
}

TEST(database_sorts_equal_difficulties_by_name)
{
    SongDatabase database;
    database.addChartFileName("pv_001_easy.dsc");
    database.addChartFileName("pv_002_easy.dsc");

    database.song(1).difficulties[0] = 20;
    database.song(2).difficulties[0] = 20;
    database.song(1).name = "Beta";
    database.song(2).name = "Alpha";

    database.sortCharts(false);

    CHECK_EQ(database.charts(0)[0], std::string("002"));
}

TEST(database_sorts_case_variants_and_duplicates_by_name)
{
    SongDatabase database;

    for (int i = 1; i <= 8; i++)
    {
        char file[32];
        snprintf(file, sizeof(file), "pv_%03d_easy.dsc", i);
        database.addChartFileName(file);
    }

    database.song(1).name = "Melt";
    database.song(2).name = "meltdown";
    database.song(3).name = "MELT";
    database.song(4).name = "39";
    database.song(5).name = "1925";
    database.song(6).name = "Ai Dee";
    database.song(7).name = "ai dee";
    database.song(8).name = "!!!";

    database.sortCharts(true);

    CHECK_EQ(database.charts(0)[0], std::string("008"));
    CHECK_EQ(database.charts(0)[1], std::string("006"));
    CHECK_EQ(database.charts(0)[2], std::string("007"));
    CHECK_EQ(database.charts(0)[3], std::string("001"));
    CHECK_EQ(database.charts(0)[4], std::string("003"));
    CHECK_EQ(database.charts(0)[5], std::string("002"));
    CHECK_EQ(database.charts(0)[6], std::string("005"));
    CHECK_EQ(database.charts(0)[7], std::string("004"));
}

TEST(database_calculates_the_ranks)
{
    Results results;
    results.total = 100;
    results.comboMax = 100;
    results.clear = 100.0f;

    // A run without a miss is a perfect one
    CHECK_EQ(SongDatabase::calculateRank(results, 0, false), 4u);

    results.comboMax = 50;
    CHECK_EQ(SongDatabase::calculateRank(results, 0, false), 3u);

    results.clear = 40.0f;
    CHECK_EQ(SongDatabase::calculateRank(results, 0, false), 1u);

    results.clear = 70.0f;
    CHECK_EQ(SongDatabase::calculateRank(results, 0, false), 2u);

    results.clear = 10.0f;
    CHECK_EQ(SongDatabase::calculateRank(results, 0, false), 0u);

    // A failed run is never better than a drop out
    results.clear = 100.0f;
    CHECK_EQ(SongDatabase::calculateRank(results, 0, true), 0u);
}
