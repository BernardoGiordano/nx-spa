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

#include <stdio.h>
#include <switch.h>

#include "bootstrap.h"
#include "logger.h"
#include "web_app.h"

namespace {
void showError(Result rc) {
  consoleInit(nullptr);
  printf("nx-spa could not start.\n\n");
  printf("Result: 0x%08X\n\n", rc);

  if (appletGetAppletType() != AppletType_Application) {
    printf("The Switch web applet requires Application mode.\n");
    printf(
        "Open hbmenu by holding R while launching a game, then run nx-spa.\n");
  } else {
    printf(
        "Check that Atmosphere is current and that the SD card is writable.\n");
    printf("nx-spa stages its offline UI under:\n");
    printf("sdmc:/atmosphere/hbl_html/html-document/nx-spa.htdocs\n");
  }

  printf("\nPress + to exit.\n");
  consoleUpdate(nullptr);

  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  PadState pad;
  padInitializeDefault(&pad);
  while (appletMainLoop()) {
    padUpdate(&pad);
    if ((padGetButtonsDown(&pad) & HidNpadButton_Plus) != 0) {
      break;
    }
    svcSleepThread(16'000'000);
  }
  consoleExit(nullptr);
}
}  // namespace

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  Result rc = appInit();
  if (R_FAILED(rc)) {
    LOG_ERROR("initialization failed: 0x%08X", rc);
    showError(rc);
    loggerExit();
    return 1;
  }

  rc = webAppRun();
  if (R_FAILED(rc)) {
    LOG_ERROR("web app failed: 0x%08X", rc);
    showError(rc);
  } else {
    LOG_INFO("web app exited normally");
  }

  appExit();
  return R_SUCCEEDED(rc) ? 0 : 1;
}
