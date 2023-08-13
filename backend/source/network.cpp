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

#include <stdexcept>
#include "network.h"

#include <switch.h>
extern "C" {
#include "mongoose.h"
}

struct Network::MongooseImpl {
  struct mg_mgr mgr;
  struct mg_connection* nc;
  struct mg_serve_http_opts httpServerOpts;

  static void event_handler(struct mg_connection* nc, int ev, void* ev_data);

  MongooseImpl() {
    mg_mgr_init(&mgr, this); // makes our data available through mgr->user_data (after an appropriate cast)
    nc = mg_bind(&mgr, NETWORK_PORT_STR, &Network::MongooseImpl::event_handler);
    mg_set_protocol_http_websocket(nc);
    httpServerOpts.document_root = "romfs:/webapp";
  }
  ~MongooseImpl() {
    mg_mgr_free(&mgr);
  }
};

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

void Network::MongooseImpl::event_handler(struct mg_connection* nc, int ev, void* ev_data) {
    struct http_message* hm = (struct http_message*)ev_data;
    auto& self = *(Network::MongooseImpl*)(nc->mgr->user_data);

    switch (ev) {
      case MG_EV_HTTP_REQUEST:
        if (mg_vcmp(&hm->uri, "/sum") == 0) {
          handle_sum_call(nc, hm);
        } else {
          mg_serve_http(nc, hm, self.httpServerOpts);
        }
        break;
      default:
        break;
    }
  }

void Network::server_loop()
{
  // keep going until flag is set
  while (!stop_flag.test()) {
    mg_mgr_poll(&pimpl->mgr, 1000 / 50);
  }
}

Network::Network()
  // imo dirty, but is an usual idiom to hide implementation
  // didn't want to include mongoose in the header
  : pimpl{std::make_unique<MongooseImpl>()}
  // initialize as clear, the thread expects it to be cleared to keep running
  , stop_flag{}
{
  server_thread = std::thread(&Network::server_loop, this);
}
Network::~Network()
{
  stop_flag.test_and_set(); // tell the thread to stop
  server_thread.join(); // wait for the thread to complete
}
