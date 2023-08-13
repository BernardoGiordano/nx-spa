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
extern "C" {
#include "mongoose.h"
}

static bool shouldExitNetworkLoop = false;
static struct mg_mgr mgr;
static struct mg_connection* nc;
static struct mg_serve_http_opts httpServerOpts;
static const char* httpServerPort = "8080";
static Thread networkThread;

static void pollServer(void) { mg_mgr_poll(&mgr, 1000 / 60); }

static void webServerThreadFunc(void) {
  while (!shouldExitNetworkLoop) {
    pollServer();
  }
}

static void handle_sum_call(struct mg_connection *nc, struct http_message *hm) {
  char n1[100], n2[100];
  double result;

  /* Get variables */
  mg_get_http_var(&hm->query_string, "n1", n1, sizeof(n1));
  mg_get_http_var(&hm->query_string, "n2", n2, sizeof(n2));

  /* Send headers */
  mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");

  /* Compute the result and send it back as a JSON object */
  result = strtod(n1, NULL) + strtod(n2, NULL);
  mg_printf_http_chunk(nc, "{ \"result\": %lf }", result);
  mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
}

static void ev_handler(struct mg_connection* nc, int ev, void* ev_data) {
  struct http_message* hm = (struct http_message*)ev_data;

  switch (ev) {
    case MG_EV_HTTP_REQUEST:
      if (mg_vcmp(&hm->uri, "/sum") == 0) {
        handle_sum_call(nc, hm);
      } else {
        mg_serve_http(nc, hm, httpServerOpts);
      }
      break;
    default:
      break;
  }
}

static void initWebServer(void) {
  mg_mgr_init(&mgr, NULL);
  nc = mg_bind(&mgr, httpServerPort, ev_handler);
  mg_set_protocol_http_websocket(nc);
  httpServerOpts.document_root = "romfs:/webapp";
}

bool networkInit() {
  Result res;
  if ((res = socketInitializeDefault()) == 0) {
    nxlinkStdio();
  } else {
    shouldExitNetworkLoop = true;
    return false;
  }

  initWebServer();

  threadCreate(&networkThread, (ThreadFunc)webServerThreadFunc, nullptr,
               nullptr, 16 * 1000, 0x2C, -2);
  threadStart(&networkThread);

  return true;
}

void networkExit() {
  shouldExitNetworkLoop = true;
  threadWaitForExit(&networkThread);
  threadClose(&networkThread);
  mg_mgr_free(&mgr);
  socketExit();
}