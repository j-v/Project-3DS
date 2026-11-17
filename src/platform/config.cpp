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

#include "config.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

namespace p3ds {

/** The root of the SD card. */
static const char *SdRoot = "sdmc:";

/** The name of the configuration file at the root of the SD card. */
static const char *ConfigFile = "sdmc:/project-3ds.cfg";

/** The default data root - same directory Project-DS uses. */
static const char *DefaultRoot = "/project-ds";

/** Creates a directory and every missing parent below the SD card root. */
static void makeDirectory(const std::string &path)
{
    // mkdir does not create the parents, so each component is made in turn.
    // A component that is already there is expected and ignored.
    for (size_t i = 1; i <= path.size(); i++)
    {
        if (i != path.size() && path[i] != '/')
            continue;

        std::string part = path.substr(0, i);

        if (!part.empty() && part != "sdmc:" && part != "sdmc:/")
            mkdir(part.c_str(), 0777);
    }
}

/** The file name suffix of each difficulty. */
static const char *ChartEnds[5] =
{
    "_easy.dsc",
    "_normal.dsc",
    "_hard.dsc",
    "_extreme.dsc",
    "_extreme_1.dsc"
};

Config::Config()
{
    root = DefaultRoot;
}

void Config::load()
{
    FILE *file = fopen(ConfigFile, "r");

    if (!file)
        return;

    char line[256];

    while (fgets(line, sizeof(line), file))
    {
        std::string str = line;

        // Remove the line ending
        while (!str.empty() && (str[str.size() - 1] == '\n' || str[str.size() - 1] == '\r'))
            str.erase(str.size() - 1);

        // Read datadir setting
        if (str.compare(0, 7, "datadir=") == 0)
        {
            std::string value = str.substr(7);

            // The device is added by dataPath, so a value that carries it is
            // normalized instead of producing sdmc:sdmc:
            if (value.compare(0, 5, "sdmc:") == 0)
                value.erase(0, 5);

            // A path that is not empty is used as it is, with a leading slash
            if (!value.empty())
            {
                if (value[0] != '/')
                    value = "/" + value;

                this->root = value;
            }
        }
    }

    fclose(file);
}

std::string Config::rootPath() const
{
    std::string path = SdRoot + root;

    // A trailing slash would make dataPath add another one
    while (path.size() > 5 && path[path.size() - 1] == '/')
        path.erase(path.size() - 1);

    return path;
}

std::string Config::dataPath(const std::string &sub) const
{
    std::string path = rootPath();

    if (!sub.empty())
        path += "/" + sub;

    return path;
}

std::string Config::chartPath(const std::string &id, int difficulty) const
{
    return dataPath("dsc/pv_" + id + ChartEnds[difficulty < 0 || difficulty > 4 ? 0 : difficulty]);
}

std::string Config::songPath(const std::string &id) const
{
    return dataPath("ogg/pv_" + id + ".ogg");
}

std::string Config::scoresPath() const
{
    return dataPath("scores.txt");
}

void Config::createFolders()
{
    // Create the directories in case they don't exist. The database is read
    // as well as the charts and the music, so it is created too.
    makeDirectory(rootPath());
    makeDirectory(dataPath("dsc"));
    makeDirectory(dataPath("ogg"));
    makeDirectory(dataPath("db"));
}

} // namespace p3ds
