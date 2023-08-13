/*
 *   This file is part of nx-spa
 *   Copyright (C) 2023 Bernardo Giordano
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

#include <switch.h>
#include <stdexcept>
#include <cinttypes>
#include "bootstrap.h"

/*
 * when using regular network:
 *  - known to work when internet connection available
 *  - known to not work when in airplane mode
 * when using ldn (local) network:
 *  - ?
 */
// comment out to prevent ldn usage
#define USE_LOCALNET

AppServices::AppServices() {
  char buf[128] = {0};
  Result res = 0;

  res = romfsInit();
  if(R_FAILED(res))
  {
    snprintf(buf, sizeof(buf), "romfsInit failure: %" PRIx32 "\n", res);
    throw std::runtime_error(std::string(buf));
  }

  res = socketInitializeDefault();
  if(R_FAILED(res))
  {
    snprintf(buf, sizeof(buf), "socketInitializeDefault failure: %" PRIx32 "\n", res);
    romfsExit();
    throw std::runtime_error(std::string(buf));
  }

#ifdef USE_LOCALNET
  res = ldnInitialize(LdnServiceType_User);
  if(R_FAILED(res))
  {
    snprintf(buf, sizeof(buf), "ldnInitialize failure: %" PRIx32 "\n", res);
    socketExit();
    romfsExit();
    throw std::runtime_error(std::string(buf));
  }
#endif
}

AppServices::~AppServices() {
#ifdef USE_LOCALNET
  ldnExit();
#endif
  socketExit();
  romfsExit();
}
