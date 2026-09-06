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
 */

#include "web_app.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <cmath>
#include <new>

#include "logger.h"

namespace {
constexpr const char* SourceRoot = "romfs:/webapp";
constexpr const char* HtmlRoot = "sdmc:/atmosphere/hbl_html";
constexpr const char* HtmlDocumentRoot =
    "sdmc:/atmosphere/hbl_html/html-document";
constexpr const char* DocumentRoot =
    "sdmc:/atmosphere/hbl_html/html-document/nx-spa.htdocs";
constexpr const char* LegacyDocumentRoot =
    "sdmc:/atmosphere/hbl_html/nx-spa.htdocs";
constexpr const char* DocumentPath = "nx-spa.htdocs/index.html";

constexpr size_t PathCapacity = 0x400;
constexpr size_t CopyBufferSize = 64 * 1024;
// WebSession allows 0x8000 bytes per browser-content storage, including its
// 0x10-byte header.
constexpr size_t MessageCapacity = 0x7FF0;
constexpr u64 PollIntervalNs = 16'000'000;
constexpr u64 AppearTimeoutNs = 8'000'000'000;
constexpr u64 ReadyRetryIntervalNs = 500'000'000;
constexpr unsigned ReadyRetryLimit = 20;
// The browser does not pop a lone message left in the WebSession channel once
// the page goes idle; the next arriving message flushes the previous one. So
// native keeps a pump running: fast while a reply is in flight, slow at rest.
constexpr u64 FlushIntervalNs = 100'000'000;
constexpr u64 IdleHeartbeatNs = 2'000'000'000;
constexpr u64 PumpWindowNs = 3'000'000'000;
constexpr u64 ActivityIntervalNs = 30'000'000'000;
constexpr u64 ExitTimeoutNs = 5'000'000'000;

Result ioError() {
  const Result rc = fsdevGetLastResult();
  return R_FAILED(rc) ? rc : MAKERESULT(Module_Libnx, LibnxError_IoError);
}

Result makeDirectory(const char* path) {
  if (mkdir(path, 0777) == 0 || errno == EEXIST) {
    return 0;
  }
  return ioError();
}

Result joinPath(char* output, size_t outputSize, const char* parent,
                const char* child) {
  const int written = snprintf(output, outputSize, "%s/%s", parent, child);
  if (written < 0 || static_cast<size_t>(written) >= outputSize) {
    return MAKERESULT(Module_Libnx, LibnxError_BadInput);
  }
  return 0;
}

Result copyFile(const char* source, const char* destination) {
  LOG_DEBUG("staging %s -> %s", source, destination);
  FILE* input = fopen(source, "rb");
  if (input == nullptr) {
    return ioError();
  }

  FILE* output = fopen(destination, "wb");
  if (output == nullptr) {
    fclose(input);
    return ioError();
  }

  Result rc = 0;
  auto* buffer = new (std::nothrow) unsigned char[CopyBufferSize];
  if (buffer == nullptr) {
    rc = MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
  }

  while (R_SUCCEEDED(rc) && !feof(input)) {
    const size_t bytesRead = fread(buffer, 1, CopyBufferSize, input);
    if (bytesRead != 0 && fwrite(buffer, 1, bytesRead, output) != bytesRead) {
      rc = ioError();
    }
    if (ferror(input)) {
      rc = ioError();
    }
  }

  delete[] buffer;
  if (fclose(output) != 0 && R_SUCCEEDED(rc)) {
    rc = ioError();
  }
  fclose(input);
  return rc;
}

Result copyDirectory(const char* source, const char* destination) {
  Result rc = makeDirectory(destination);
  if (R_FAILED(rc)) {
    return rc;
  }

  DIR* directory = opendir(source);
  if (directory == nullptr) {
    return ioError();
  }

  while (R_SUCCEEDED(rc)) {
    errno = 0;
    dirent* entry = readdir(directory);
    if (entry == nullptr) {
      if (errno != 0) {
        rc = ioError();
      }
      break;
    }
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    char sourcePath[PathCapacity];
    char destinationPath[PathCapacity];
    rc = joinPath(sourcePath, sizeof(sourcePath), source, entry->d_name);
    if (R_SUCCEEDED(rc)) {
      rc = joinPath(destinationPath, sizeof(destinationPath), destination,
                    entry->d_name);
    }
    if (R_FAILED(rc)) {
      break;
    }

    struct stat info{};
    if (stat(sourcePath, &info) != 0) {
      rc = ioError();
    } else if (S_ISDIR(info.st_mode)) {
      rc = copyDirectory(sourcePath, destinationPath);
    } else if (S_ISREG(info.st_mode)) {
      rc = copyFile(sourcePath, destinationPath);
    }
  }

  closedir(directory);
  return rc;
}

Result removeDirectoryIfPresent(const char* path) {
  struct stat info{};
  if (stat(path, &info) != 0) {
    return errno == ENOENT ? 0 : ioError();
  }
  if (!S_ISDIR(info.st_mode)) {
    return MAKERESULT(Module_Libnx, LibnxError_BadInput);
  }
  return fsdevDeleteDirectoryRecursively(path);
}

Result stageWebApp() {
  LOG_INFO("staging offline frontend at %s", DocumentRoot);
  Result rc = makeDirectory("sdmc:/atmosphere");
  if (R_SUCCEEDED(rc)) {
    rc = makeDirectory(HtmlRoot);
  }
  if (R_SUCCEEDED(rc)) {
    rc = makeDirectory(HtmlDocumentRoot);
  }
  if (R_SUCCEEDED(rc)) {
    rc = removeDirectoryIfPresent(LegacyDocumentRoot);
  }
  if (R_SUCCEEDED(rc)) {
    rc = removeDirectoryIfPresent(DocumentRoot);
  }
  if (R_SUCCEEDED(rc)) {
    rc = copyDirectory(SourceRoot, DocumentRoot);
  }
  if (R_SUCCEEDED(rc)) {
    rc = fsdevCommitDevice("sdmc");
  }
  if (R_SUCCEEDED(rc)) {
    LOG_INFO("offline frontend staged and committed");
  } else {
    LOG_ERROR("frontend staging failed: 0x%08X", rc);
  }
  return rc;
}

const char* findJsonValue(const char* json, const char* name) {
  char needle[64];
  const int length = snprintf(needle, sizeof(needle), "\"%s\"", name);
  if (length < 0 || static_cast<size_t>(length) >= sizeof(needle)) {
    return nullptr;
  }

  const char* value = strstr(json, needle);
  if (value == nullptr) {
    return nullptr;
  }
  value = strchr(value + length, ':');
  if (value == nullptr) {
    return nullptr;
  }
  do {
    ++value;
  } while (*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n');
  return value;
}

bool readJsonString(const char* json, const char* name, char* output,
                    size_t outputSize) {
  const char* value = findJsonValue(json, name);
  if (value == nullptr || *value != '"' || outputSize == 0) {
    return false;
  }

  ++value;
  size_t outputIndex = 0;
  while (*value != '\0' && *value != '"') {
    if (*value == '\\') {
      ++value;
      if (*value == '\0') {
        return false;
      }
    }
    if (outputIndex + 1 >= outputSize) {
      return false;
    }
    output[outputIndex++] = *value++;
  }
  if (*value != '"') {
    return false;
  }
  output[outputIndex] = '\0';
  return true;
}

bool readJsonUnsigned(const char* json, const char* name, u64* output) {
  const char* value = findJsonValue(json, name);
  if (value == nullptr || *value < '0' || *value > '9') {
    return false;
  }

  char* end = nullptr;
  const unsigned long long parsed = strtoull(value, &end, 10);
  if (end == value) {
    return false;
  }
  *output = parsed;
  return true;
}

bool readJsonNumber(const char* json, const char* name, double* output) {
  const char* value = findJsonValue(json, name);
  if (value == nullptr) {
    return false;
  }

  char* end = nullptr;
  const double parsed = strtod(value, &end);
  if (end == value || !std::isfinite(parsed)) {
    return false;
  }
  *output = parsed;
  return true;
}

Result sendMessage(WebSession* session, const char* message,
                   bool quiet = false) {
  const size_t messageSize = strlen(message) + 1;
  if (messageSize > MessageCapacity) {
    LOG_ERROR("native -> browser message is too large (%zu bytes)",
              messageSize);
    return MAKERESULT(Module_Libnx, LibnxError_BadInput);
  }

  for (unsigned attempt = 0; attempt < 200; ++attempt) {
    bool sent = false;
    const Result rc = webSessionTrySendContentMessage(
        session, message, static_cast<u32>(messageSize), &sent);
    if (R_FAILED(rc)) {
      LOG_ERROR("webSessionTrySendContentMessage failed: 0x%08X", rc);
      return rc;
    }
    if (sent) {
      if (!quiet) {
        LOG_DEBUG("native -> browser (%zu bytes): %.512s", messageSize,
                  message);
      }
      return 0;
    }
    if (attempt == 0) {
      LOG_WARN("native -> browser queue is full; retrying");
    }
    svcSleepThread(10'000'000);
  }
  LOG_ERROR("native -> browser queue stayed full for 2 seconds");
  return MAKERESULT(Module_Libnx, LibnxError_Timeout);
}

Result sendRpcError(WebSession* session, u64 id, int code,
                    const char* message) {
  char response[512];
  snprintf(response, sizeof(response),
           "{\"jsonrpc\":\"2.0\",\"id\":%llu,\"error\":{\"code\":%d,"
           "\"message\":\"%s\"}}",
           static_cast<unsigned long long>(id), code, message);
  return sendMessage(session, response);
}

Result dispatchRpc(WebSession* session, const char* request, bool* pageReady,
                   bool* bridgeConnected) {
  char method[64];
  if (!readJsonString(request, "method", method, sizeof(method))) {
    LOG_WARN("ignoring message without a valid method");
    return 0;
  }

  if (strcmp(method, "app.ready") == 0) {
    *pageReady = true;
    LOG_INFO("frontend reported app.ready");
    return 0;
  }

  if (strcmp(method, "dev.log") == 0) {
    char text[512];
    if (readJsonString(request, "text", text, sizeof(text))) {
      LOG_INFO("frontend: %s", text);
    }
    return 0;
  }

  if (strcmp(method, "app.connected") == 0) {
    *bridgeConnected = true;
    LOG_INFO("frontend confirmed native bridge connection");
    return 0;
  }

  u64 id = 0;
  if (!readJsonUnsigned(request, "id", &id)) {
    LOG_WARN("ignoring notification for unknown method %s", method);
    return 0;
  }

  LOG_INFO("RPC request id=%llu method=%s", static_cast<unsigned long long>(id),
           method);

  char response[1024];
  if (strcmp(method, "math.sum") == 0) {
    double a = 0.0;
    double b = 0.0;
    if (!readJsonNumber(request, "a", &a) ||
        !readJsonNumber(request, "b", &b)) {
      LOG_WARN("math.sum id=%llu has invalid parameters",
               static_cast<unsigned long long>(id));
      return sendRpcError(session, id, -32602, "Expected numeric a and b");
    }
    LOG_INFO("math.sum id=%llu: %.15g + %.15g = %.15g",
             static_cast<unsigned long long>(id), a, b, a + b);
    snprintf(response, sizeof(response),
             "{\"jsonrpc\":\"2.0\",\"id\":%llu,\"result\":%.15g}",
             static_cast<unsigned long long>(id), a + b);
    return sendMessage(session, response);
  }

  if (strcmp(method, "system.info") == 0) {
    const u32 version = hosversionGet();
    snprintf(response, sizeof(response),
             "{\"jsonrpc\":\"2.0\",\"id\":%llu,\"result\":{"
             "\"firmware\":\"%u.%u.%u\",\"atmosphere\":%s,"
             "\"offline\":true,\"transport\":\"WebSession\"}}",
             static_cast<unsigned long long>(id), HOSVER_MAJOR(version),
             HOSVER_MINOR(version), HOSVER_MICRO(version),
             hosversionIsAtmosphere() ? "true" : "false");
    return sendMessage(session, response);
  }

  return sendRpcError(session, id, -32601, "Method not found");
}

struct LifecycleState {
  bool resumed;
  bool exitRequested;
};

void handleLifecycle(AppletHookType hook, void* parameter) {
  auto* state = static_cast<LifecycleState*>(parameter);
  if (hook == AppletHookType_OnResume) {
    state->resumed = true;
  } else if (hook == AppletHookType_OnExitRequest) {
    state->exitRequested = true;
  }
}

class SleepGuard {
 public:
  SleepGuard() {
    bool disabled = false;
    if (R_SUCCEEDED(appletIsAutoSleepDisabled(&disabled))) {
      previousDisabled_ = disabled;
    }
    held_ = R_SUCCEEDED(appletSetAutoSleepDisabled(true));
    LOG_INFO("auto-sleep guard %s (previously %s)",
             held_ ? "enabled" : "could not be enabled",
             previousDisabled_ ? "disabled" : "enabled");
  }

  ~SleepGuard() {
    if (held_) {
      appletSetAutoSleepDisabled(previousDisabled_);
      LOG_INFO("auto-sleep setting restored");
    }
  }

  SleepGuard(const SleepGuard&) = delete;
  SleepGuard& operator=(const SleepGuard&) = delete;

 private:
  bool held_ = false;
  bool previousDisabled_ = false;
};

Result configureWebApp(WebCommonConfig* config) {
  LOG_INFO("configuring offline WebSession for %s", DocumentPath);
  Result rc = webOfflineCreate(config, WebDocumentKind_OfflineHtmlPage, 0,
                               DocumentPath);
  if (R_SUCCEEDED(rc)) {
    rc = webConfigSetFooter(config, false);
  }
  if (R_SUCCEEDED(rc)) {
    rc = webConfigSetFooterFixedKind(config, WebFooterFixedKind_Hidden);
  }
  if (R_SUCCEEDED(rc)) {
    rc = webConfigSetPointer(config, true);
  }
  if (R_SUCCEEDED(rc)) {
    rc = webConfigSetLeftStickMode(config, WebLeftStickMode_Pointer);
  }
  if (R_SUCCEEDED(rc)) {
    rc = webConfigSetTouchEnabledOnContents(config, true);
  }
  if (R_SUCCEEDED(rc)) {
    rc = webConfigSetJsExtension(config, true);
  }
  if (R_SUCCEEDED(rc)) {
    rc = webConfigSetBootDisplayKind(config, WebBootDisplayKind_Black);
  }
  if (R_SUCCEEDED(rc)) {
    rc = webConfigSetPageCache(config, false);
  }
  if (R_SUCCEEDED(rc)) {
    rc = webConfigSetBootLoadingIcon(config, false);
  }
  if (R_SUCCEEDED(rc)) {
    rc = webConfigSetBootMode(config,
                              WebSessionBootMode_AllForegroundInitiallyHidden);
  }
  return rc;
}
}  // namespace

Result webAppRun() {
  LOG_INFO("starting web application");
  if (appletGetAppletType() != AppletType_Application) {
    LOG_ERROR("Application mode is required (applet type=%d)",
              static_cast<int>(appletGetAppletType()));
    return MAKERESULT(Module_Libnx, LibnxError_AppletFailedToInitialize);
  }
  if (hosversionBefore(7, 0, 0)) {
    LOG_ERROR("WebSession requires HOS 7.0.0 or newer");
    return MAKERESULT(Module_Libnx, LibnxError_IncompatSysVer);
  }

  Result rc = stageWebApp();
  if (R_FAILED(rc)) {
    return rc;
  }

  SleepGuard sleepGuard;
  rc = appletLockExit();
  if (R_FAILED(rc)) {
    LOG_ERROR("appletLockExit failed: 0x%08X", rc);
    return rc;
  }

  WebCommonConfig config{};
  rc = configureWebApp(&config);
  if (R_FAILED(rc)) {
    LOG_ERROR("WebSession configuration failed: 0x%08X", rc);
    appletUnlockExit();
    return rc;
  }

  WebSession session{};
  webSessionCreate(&session, &config);

  Event* exitEvent = nullptr;
  rc = webSessionStart(&session, &exitEvent);
  if (R_FAILED(rc)) {
    LOG_ERROR("webSessionStart failed: 0x%08X", rc);
    webSessionClose(&session);
    appletUnlockExit();
    return rc;
  }
  LOG_INFO("WebSession started hidden; waiting for frontend readiness");

  LifecycleState lifecycle{};
  AppletHookCookie lifecycleCookie{};
  appletHook(&lifecycleCookie, handleLifecycle, &lifecycle);

  bool appeared = false;
  bool pageReady = false;
  bool bridgeConnected = false;
  bool requestedExit = false;
  unsigned readyAttempts = 0;
  unsigned pumpSeq = 0;
  const u64 startedAt = armTicksToNs(armGetSystemTick());
  u64 lastActivity = startedAt;
  u64 lastBridgeReady = 0;
  u64 nextPump = 0;
  u64 pumpUntil = 0;
  char message[MessageCapacity];

  while (R_FAILED(eventWait(exitEvent, 0))) {
    if (!appletMainLoop() || lifecycle.exitRequested) {
      LOG_INFO("application exit requested");
      requestedExit = true;
      rc = webSessionRequestExit(&session);
      break;
    }

    bool received = false;
    bool sawBrowserMessage = false;
    do {
      u64 messageSize = 0;
      rc = webSessionTryReceiveContentMessage(
          &session, message, sizeof(message), &messageSize, &received);
      if (R_FAILED(rc)) {
        LOG_ERROR("webSessionTryReceiveContentMessage failed: 0x%08X", rc);
        break;
      }
      if (received) {
        if (messageSize == 0 || messageSize > sizeof(message)) {
          // out_size is the pre-clamp size, so anything larger arrived
          // truncated. Drop that message instead of ending the session.
          LOG_WARN("dropping browser message with unusable size: %llu",
                   static_cast<unsigned long long>(messageSize));
          continue;
        }
        LOG_DEBUG("browser -> native (%llu bytes): %.512s",
                  static_cast<unsigned long long>(messageSize), message);
        sawBrowserMessage = true;
        rc = dispatchRpc(&session, message, &pageReady, &bridgeConnected);

        // Do not drain calls queued immediately behind app.ready until the
        // applet has appeared and native has completed the ready handshake.
        if (pageReady && !appeared) {
          break;
        }
      }
    } while (R_SUCCEEDED(rc) && received);
    if (R_FAILED(rc)) {
      LOG_ERROR("ending WebSession after bridge failure: 0x%08X", rc);
      requestedExit = true;
      webSessionRequestExit(&session);
      break;
    }

    const u64 now = armTicksToNs(armGetSystemTick());
    if (sawBrowserMessage) {
      // The drain loop always exits with received == false, so track the
      // traffic separately. A request implies a reply is on its way out, and
      // any browser traffic means the page is awake. Both are worth pumping
      // hard for a while.
      pumpUntil = now + PumpWindowNs;
    }

    if (!appeared && (pageReady || now - startedAt >= AppearTimeoutNs)) {
      if (!pageReady) {
        LOG_WARN("frontend readiness timed out; showing diagnostics page");
      }
      bool sent = false;
      rc = webSessionAppear(&session, &sent);
      appeared = R_SUCCEEDED(rc) && sent;
      if (appeared) {
        LOG_INFO("WebSession appeared");
      }
      if (R_FAILED(rc)) {
        LOG_ERROR("webSessionAppear failed: 0x%08X", rc);
        requestedExit = true;
        webSessionRequestExit(&session);
        break;
      }
    }

    if (appeared && pageReady && !bridgeConnected &&
        readyAttempts < ReadyRetryLimit &&
        (readyAttempts == 0 || now - lastBridgeReady >= ReadyRetryIntervalNs)) {
      rc = sendMessage(
          &session,
          "{\"jsonrpc\":\"2.0\",\"method\":\"app.ready\",\"params\":{"
          "\"transport\":\"WebSession\"}}");
      if (R_FAILED(rc)) {
        requestedExit = true;
        webSessionRequestExit(&session);
        break;
      }
      ++readyAttempts;
      lastBridgeReady = now;
      LOG_INFO("sent native bridge readiness (attempt %u/%u)", readyAttempts,
               ReadyRetryLimit);
      if (readyAttempts == ReadyRetryLimit) {
        LOG_WARN(
            "frontend never confirmed the bridge; serving requests anyway");
      }
    }

    if (appeared && now >= nextPump) {
      const bool flushing = now < pumpUntil;
      nextPump = now + (flushing ? FlushIntervalNs : IdleHeartbeatNs);
      ++pumpSeq;
      char pump[128];
      snprintf(pump, sizeof(pump),
               "{\"jsonrpc\":\"2.0\",\"method\":\"app.ping\",\"params\":{"
               "\"seq\":%u}}",
               pumpSeq);
      sendMessage(&session, pump, true);
    }

    if (lifecycle.resumed) {
      lifecycle.resumed = false;
      LOG_INFO("application resumed");
      appletReportUserIsActive();
      sendMessage(&session,
                  "{\"jsonrpc\":\"2.0\",\"method\":"
                  "\"app.lifecycle\",\"params\":{\"state\":\"resume\"}}");
    }
    if (now - lastActivity >= ActivityIntervalNs) {
      appletReportUserIsActive();
      LOG_DEBUG("reported user activity");
      lastActivity = now;
    }

    svcSleepThread(PollIntervalNs);
  }

  if (requestedExit && R_FAILED(eventWait(exitEvent, ExitTimeoutNs))) {
    LOG_WARN("WebSession did not exit in time; requesting termination");
    const Result exitResult =
        appletHolderRequestExitOrTerminate(&config.holder, ExitTimeoutNs);
    if (R_SUCCEEDED(rc) && R_FAILED(exitResult)) {
      rc = exitResult;
    }
  }

  WebCommonReply reply{};
  const Result waitResult = webSessionWaitForExit(&session, &reply);
  LOG_INFO("WebSession exited (wait result=0x%08X)", waitResult);
  webSessionClose(&session);
  appletUnhook(&lifecycleCookie);
  appletUnlockExit();
  return R_FAILED(rc) ? rc : waitResult;
}
