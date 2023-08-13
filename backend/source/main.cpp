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

#include <stdexcept> // std::exception
#include <cstdlib> // EXIT_FAILURE
#include <cstdio> // printf

#include <switch.h> // libnx

// our code
#include "bootstrap.h"
#include "network.h"

int main(int argc, char* argv[]) {
  try {
    AppServices app;
    Network net;

    WebCommonConfig config;
    Result rc = webPageCreate(&config, "http://0.0.0.0:" NETWORK_PORT_STR);
    if (R_SUCCEEDED(rc)) {
      webConfigSetWhitelist(&config, "^http*");
      webConfigSetFooter(&config, false);
      webConfigSetFooterFixedKind(&config, WebFooterFixedKind_Hidden);
      webConfigSetTouchEnabledOnContents(&config, true);
      webConfigSetJsExtension(&config, true);
      rc = webConfigShow(&config, nullptr);
    }
  }
  catch(const std::exception& exc) {
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    printf("A nx-spa error occured. Press Y to exit.\n\n");
    printf("Error: '%s'\n", exc.what());

    while(appletMainLoop())
    {
      padUpdate(&pad);

      const u32 kDown = padGetButtonsDown(&pad);
      if (kDown & HidNpadButton_Y)
        break;

      consoleUpdate(NULL);
    }

    consoleExit(NULL);
    return EXIT_FAILURE;
  }
}
