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

#include "io.h"

#include <dirent.h>
#include <stdio.h>
#include <switch.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFFER_SIZE 8192

void createDirectoryRecursively(const std::string& _p) {
  // skip first slash
  size_t pos = _p.find('/', 0) + 1;
  while ((pos = _p.find('/', pos)) != _p.npos) {
    mkdir(_p.substr(0, pos).c_str(), 777);
    ++pos;
  }
}

dirItem::dirItem(const std::string& pathTo, const std::string& sItem) {
  item = sItem;

  std::string fullPath = pathTo + sItem;
  struct stat s;
  if (stat(fullPath.c_str(), &s) == 0 && S_ISDIR(s.st_mode)) dir = true;
}

dirList::dirList(const std::string& _path) {
  DIR* d;
  struct dirent* ent;
  path = _path;
  d = opendir(path.c_str());

  while ((ent = readdir(d))) {
    if (ent->d_name[0] == '.') {
      continue;
    }

    item.emplace_back(path, ent->d_name);
  }

  closedir(d);
}

void copyFile(const std::string& srcPath, const std::string& dstPath) {
  FILE* src = fopen(srcPath.c_str(), "rb");
  if (src == NULL) {
    return;
  }
  FILE* dst = fopen(dstPath.c_str(), "wb");
  if (dst == NULL) {
    fclose(src);
    return;
  }

  fseek(src, 0, SEEK_END);
  u64 sz = ftell(src);
  rewind(src);

  u8* buf = new u8[BUFFER_SIZE];
  u64 offset = 0;

  while (offset < sz) {
    u32 count = fread((char*)buf, 1, BUFFER_SIZE, src);
    fwrite((char*)buf, 1, count, dst);
    offset += count;
  }

  delete[] buf;
  fclose(src);
  fclose(dst);
}

void copyDirectoryRecursively(const std::string& src, const std::string& dst) {
  dirList list(src);
  for (unsigned i = 0; i < list.getCount(); i++) {
    if (list.isDir(i)) {
      std::string newSrc = src + list.getItem(i) + "/";
      std::string newDst = dst + list.getItem(i);

      mkdir(newDst.c_str(), 777);
      copyDirectoryRecursively(newSrc, newDst + "/");
    } else {
      std::string fullSrc = src + list.getItem(i);
      std::string fullDst = dst + list.getItem(i);

      copyFile(fullSrc, fullDst);
    }
  }
}