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

#ifndef P3DS_CONFIG_H
#define P3DS_CONFIG_H

#include <string>

namespace p3ds {

/**
 * Where the game keeps its data.
 *
 * The data root defaults to /project-ds, and can be changed with a small text
 * file called project-3ds.cfg at the root of the SD card that holds a
 * datadir= line.
 */
class Config
{
public:
    Config();

    /** Reads the configuration file. */
    void load();

    /** The root of the data folders. */
    const std::string &dataRoot() const { return root; }

    /** A path inside the data root. */
    std::string dataPath(const std::string &sub) const;

    /** The path of a chart file. */
    std::string chartPath(const std::string &id, int difficulty) const;

    /** The path of a song file. */
    std::string songPath(const std::string &id) const;

    /** The path of the file the scores are saved in. */
    std::string scoresPath() const;

    /** Creates the data folders if they are not there yet. */
    void createFolders();

private:
    /** The full path of the data root on the SD card. */
    std::string rootPath() const;

    std::string root;
};

} // namespace p3ds

#endif // P3DS_CONFIG_H
