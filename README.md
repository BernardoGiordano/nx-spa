# nx-spa

A proof of concept for building a Nintendo Switch homebrew application with an
Angular or Vue frontend and a native C++ backend.

The application is self-contained at runtime: frontend assets are embedded in
the NRO, the Switch offline web applet renders them, and dynamic calls use
libnx `WebSession` messages. A normal SD-card launch does not open a socket and
does not require localhost, Wi-Fi, a LAN server, DNS redirection, or an
external proxy. An nxlink launch can optionally use the host connection for
development logs only.

## Architecture

```text
Angular / Vue build
        ↓
NRO RomFS: romfs:/webapp
        ↓ staged at startup
sdmc:/atmosphere/hbl_html/html-document/nx-spa.htdocs
        ↓ Atmosphere HtmlDocument redirect
Switch offline web applet
        ⇄ window.nx / libnx WebSession
native C++ RPC handlers
```

An NRO's RomFS belongs to the homebrew process and cannot be mounted directly
by the separate browser process. Atmosphere solves this for homebrew by mapping
HtmlDocument requests to `sdmc:/atmosphere/hbl_html`. nx-spa copies only its
own bundled frontend into the namespaced
`html-document/nx-spa.htdocs` directory before opening the applet.

## Requirements

- `devkitA64`, current `libnx`, `make`, Node.js, and npm to build.
- Atmosphere 0.8.5 or newer for the homebrew HtmlDocument redirect. A current
  release is strongly recommended for recent firmware browser fixes.
- Horizon OS 7.0.0 or newer for libnx `WebSession`.
- **Application mode**. Open hbmenu by holding R while launching a game, then
  launch nx-spa. Album/applet mode cannot launch this web applet reliably.
- A writable SD card. The staged copy is stored under
  `/atmosphere/hbl_html/html-document/nx-spa.htdocs`.

For Horizon OS 21.0.0 and newer, compile with libnx 4.10.0 or newer. Nintendo
changed the TLS ABI in 21.0.0, and older homebrew binaries can crash or corrupt
memory even when their own code is otherwise correct.

## Build

Angular is the default frontend:

```bash
make
```

Build the Vue variant instead:

```bash
make vue
```

The resulting application is `backend/out/nx-spa.nro`. npm dependencies are
needed at build time only; the NRO needs no network connection at runtime.

The Angular sample intentionally uses its `switch` build configuration. It
emits ES2017 and skips Angular 14's legacy production optimizer, which is not
compatible with current Node.js releases. The bundle is larger, but remains
better suited to the Switch browser. `npm run build:production` is available
when building under one of Angular 14's supported Node.js versions.

Both frontend builds convert their self-contained entry bundles from module
scripts to ordered classic scripts at the end of the document. This avoids module loading
restrictions in the offline browser applet. A static boot screen also reports
resource and startup failures before either framework takes over the page.
The generated JavaScript is then downleveled to ES5 because the applet's
JavaScriptCore rejects block-scoped declarations such as `const` in strict
mode, even on firmware where newer browser features are otherwise present.

To send the built NRO through nxlink:

```bash
make deploy
```

The receiving hbmenu session must still be running in Application mode. The
deploy command passes `-s`, and nx-spa detects the host address supplied by
nxlink, initializes libnx sockets, and redirects stderr back to that server.
The same terminal then shows timestamped native diagnostics such as:

```text
[   0.000] [INFO ] nxlink diagnostics connected to 192.168.1.10
[   1.842] [INFO ] frontend reported app.ready
[   1.858] [INFO ] WebSession appeared
[   1.860] [INFO ] frontend confirmed native bridge connection
[   1.876] [INFO ] RPC request id=2 method=math.sum
[   1.876] [INFO ] math.sum id=2: 3 + 4 = 7
```

The nxlink connection is diagnostic-only. The webapp assets and dynamic API
continue to use the offline HtmlDocument and WebSession path, and launching
the NRO normally from the SD card does not initialize networking.

## Dynamic native API

The frontend uses a small JSON-RPC-style protocol over `window.nx`. For
example, both sample frontends make this request:

```json
{"jsonrpc":"2.0","id":1,"method":"math.sum","params":{"a":3,"b":4}}
```

The native process responds through `webSessionTrySendContentMessage`:

```json
{"jsonrpc":"2.0","id":1,"result":7}
```

Startup uses an acknowledged `app.ready` handshake. The frontend does not
issue API calls until native has made the initially hidden WebSession visible
and sent the ready acknowledgement, then it confirms receipt with
`app.connected`. Native repeats its acknowledgement until that confirmation
arrives, then gives up after a bounded number of attempts and serves requests
anyway. This prevents replies from being sent during the launch window in
which the browser may ignore native content messages, without stalling a
frontend that predates the handshake.

### Channel pump

The browser does not reliably pop a lone message left in the WebSession
channel once the page has gone idle. A reply can sit there unread until the
next message arrives and flushes it, which strands the request that is waiting
on it. Native therefore keeps a pump running: an `app.ping` notification every
100 ms for three seconds after any browser traffic, dropping to one every two
seconds at rest. Frontends must treat `app.ping` as a no-op and must not log
it, render for it, or forward it to notification listeners.

`system.info` is a second example, and native lifecycle notifications show how
the C++ side can push events without a request. Add native methods in
`dispatchRpc()` in `backend/source/web_app.cpp`, then expose a typed wrapper in
the frontend bridge:

- Angular: `src/app/nx-bridge.service.ts`
- Vue: `src/nxBridge.ts`

This transport is intended for dynamic SPA behavior backed by native services,
files, databases, devices, or other C++ logic. It replaces HTTP endpoints; it
does not embed a Node.js runtime or provide server-side rendering. Large values
should be split into chunks rather than sent as one message.

## Lifecycle and sleep

The browser starts hidden and appears after the SPA sends `app.ready`, avoiding
an exposed loading screen during normal startup. An eight-second fallback makes
the applet visible so a frontend loading error is not hidden forever.

While the web applet is open, the native process:

- preserves and temporarily disables the console's auto-sleep setting;
- periodically reports user activity as a fallback;
- processes applet exit requests and closes `WebSession` cleanly;
- force-terminates a web applet that ignores a shutdown request after a bounded
  grace period, rather than waiting indefinitely;
- sends an `app.lifecycle` notification after resume; and
- restores the previous auto-sleep setting on exit.

This prevents inactivity sleep during a session. A user can still force sleep
with the power button, and the OS or custom firmware can still terminate a
library applet; application code must treat resume and applet exit as normal
lifecycle events.

## References

- [libnx web applet API](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/applets/web.h)
- [Switchbrew browser and `window.nx` notes](https://switchbrew.org/wiki/Internet_Browser)
- [Atmosphere changelog (`hbl_html` support)](https://github.com/Atmosphere-NX/Atmosphere/blob/master/docs/changelog.md)

## Credits

The original 2023 HTTP-server experiment borrowed code from
[Checkpoint](https://github.com/BernardoGiordano/Checkpoint). The current
version uses the offline HtmlDocument and WebSession facilities provided by
Atmosphere and libnx.
