/*
 *   This file is part of nx-spa
 *   Copyright (C) 2026 Bernardo Giordano
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
 */

#include "logger.h"

#include <arpa/inet.h>
#include <stdarg.h>
#include <stdio.h>
#include <switch.h>
#include <unistd.h>

namespace {
int nxlinkSocket = -1;
bool socketsInitialized = false;
u64 startedAt = 0;
}  // namespace

void loggerInit() {
  startedAt = armTicksToNs(armGetSystemTick());

  // nxlinkSetup() leaves this at zero for a normal hbmenu/SD launch.
  if (__nxlink_host.s_addr == 0) {
    return;
  }

  const Result rc = socketInitializeDefault();
  if (R_FAILED(rc)) {
    return;
  }
  socketsInitialized = true;

  // Keep stdout free for a future on-device console and route diagnostics only.
  nxlinkSocket = nxlinkStdioForDebug();
  if (nxlinkSocket < 0) {
    socketExit();
    socketsInitialized = false;
    return;
  }

  setvbuf(stderr, nullptr, _IONBF, 0);
  LOG_INFO("nxlink diagnostics connected to %s", inet_ntoa(__nxlink_host));
}

void loggerExit() {
  if (nxlinkSocket >= 0) {
    LOG_INFO("closing nxlink diagnostics");
    fflush(stderr);
    close(nxlinkSocket);
    nxlinkSocket = -1;
  }
  if (socketsInitialized) {
    socketExit();
    socketsInitialized = false;
  }
}

bool loggerIsActive() { return nxlinkSocket >= 0; }

void loggerWrite(const char* level, const char* format, ...) {
  if (nxlinkSocket < 0) {
    return;
  }

  const u64 elapsed = armTicksToNs(armGetSystemTick()) - startedAt;
  fprintf(stderr, "[%8.3f] [%-5s] ", static_cast<double>(elapsed) / 1.0e9,
          level);

  va_list arguments;
  va_start(arguments, format);
  vfprintf(stderr, format, arguments);
  va_end(arguments);
  fputc('\n', stderr);
}
