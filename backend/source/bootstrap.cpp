/*
 *   This file is part of nx-spa
 *   Copyright (C) 2023-2026 Bernardo Giordano
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

#include "bootstrap.h"

#include "logger.h"

Result appInit() {
  loggerInit();
  LOG_INFO("nx-spa starting (HOS %u.%u.%u, Atmosphere=%s)",
           HOSVER_MAJOR(hosversionGet()), HOSVER_MINOR(hosversionGet()),
           HOSVER_MICRO(hosversionGet()),
           hosversionIsAtmosphere() ? "yes" : "no");

  const Result rc = romfsInit();
  if (R_FAILED(rc)) {
    LOG_ERROR("romfsInit failed: 0x%08X", rc);
  } else {
    LOG_INFO("RomFS mounted");
  }
  return rc;
}

void appExit() {
  LOG_INFO("unmounting RomFS");
  romfsExit();
  loggerExit();
}
