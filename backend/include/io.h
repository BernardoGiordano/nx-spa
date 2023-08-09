/*
 *   This file is part of nx-spa
 *   Copyright (C) 2023 Bernardo Giordano, J-D-K
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
 *       * Requiring preservation of specified reasonable legal notices or
 *         author attributions in that material or in the Appropriate Legal
 *         Notices displayed by works containing it.
 *       * Prohibiting misrepresentation of the origin of that material,
 *         or requiring that modified versions of such material be marked in
 *         reasonable ways as different from the original version.
 */

#ifndef BACKEND_IO_H
#define BACKEND_IO_H

#include <string>
#include <vector>

class dirItem {
 public:
  dirItem(const std::string& pathTo, const std::string& item);
  std::string getItem() const { return item; }
  bool isDir() const { return dir; }

 private:
  std::string item;
  bool dir = false;
};

class dirList {
 public:
  dirList(const std::string& path);

  std::string getItem(int index) const { return item[index].getItem(); }
  bool isDir(int index) const { return item[index].isDir(); }
  unsigned getCount() const { return item.size(); }

 private:
  std::string path;
  std::vector<dirItem> item;
};

void createDirectoryRecursively(const std::string&);
void copyFile(const std::string& srcPath, const std::string& dstPath);
void copyDirectoryRecursively(const std::string& srcPath,
                              const std::string& dstPath);

#endif  // BACKEND_IO_H