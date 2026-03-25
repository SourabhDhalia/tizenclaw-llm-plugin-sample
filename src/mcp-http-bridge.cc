/*
 * Copyright (c) 2026 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file mcp-http-bridge.cc
 * @brief Stdio ↔ HTTP MCP Bridge using tizenclaw_curl API.
 *
 * Bridges TizenClaw's stdio-based McpClient to HTTP MCP servers
 * (such as Swiggy MCP). Reads JSON-RPC from stdin, POSTs to the
 * HTTP endpoint, returns responses to stdout.
 *
 * Usage: mcp-http-bridge <url> [auth_token]
 * Example: mcp-http-bridge https://mcp.swiggy.com/food
 */

#include <dlog.h>
#include <tizenclaw/llm-backend/tizenclaw_curl.h>

#include <cstring>
#include <iostream>
#include <string>

#undef PROJECT_TAG
#define PROJECT_TAG "TIZENCLAW_MCP_BRIDGE"

namespace {

struct WriteContext {
  std::string *buffer;
};

void WriteCallback(const char *chunk, void *user_data) {
  auto *ctx = static_cast<WriteContext *>(user_data);
  if (chunk && ctx->buffer) {
    ctx->buffer->append(chunk);
  }
}

// Track MCP session ID across requests
std::string g_session_id;

/**
 * Send a JSON-RPC request to the HTTP MCP endpoint.
 * Returns the response body as a string.
 */
std::string SendHttpRequest(const std::string &url, const std::string &body,
                            const std::string &auth_token) {
  tizenclaw_curl_h curl = nullptr;
  if (tizenclaw_curl_create(&curl) != TIZENCLAW_ERROR_NONE) {
    return "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,"
           "\"message\":\"Failed to create curl handle\"},\"id\":null}";
  }

  tizenclaw_curl_set_url(curl, url.c_str());

  // Required headers for MCP Streamable HTTP transport
  tizenclaw_curl_add_header(curl, "Content-Type: application/json");
  tizenclaw_curl_add_header(curl, "Accept: application/json");

  if (!auth_token.empty()) {
    std::string auth_header = "Authorization: Bearer " + auth_token;
    tizenclaw_curl_add_header(curl, auth_header.c_str());
  }

  if (!g_session_id.empty()) {
    std::string session_header = "Mcp-Session-Id: " + g_session_id;
    tizenclaw_curl_add_header(curl, session_header.c_str());
  }

  tizenclaw_curl_set_post_data(curl, body.c_str());

  std::string response_buffer;
  WriteContext write_ctx;
  write_ctx.buffer = &response_buffer;
  tizenclaw_curl_set_write_callback(curl, WriteCallback, &write_ctx);
  tizenclaw_curl_set_timeout(curl, 10, 60);

  int res = tizenclaw_curl_perform(curl);

  if (res != TIZENCLAW_ERROR_NONE) {
    const char *err = tizenclaw_curl_get_error_message(curl);
    std::string err_msg = err ? err : "Unknown error";
    dlog_print(DLOG_ERROR, PROJECT_TAG, "HTTP request failed: %s",
               err_msg.c_str());
    tizenclaw_curl_destroy(curl);
    return "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,"
           "\"message\":\"HTTP request failed: " +
           err_msg + "\"},\"id\":null}";
  }

  long http_code = 0;
  tizenclaw_curl_get_response_code(curl, &http_code);

  // TODO: Extract Mcp-Session-Id from response headers when
  // tizenclaw_curl adds header retrieval support. For now,
  // many HTTP MCP servers include session info in the JSON body.

  tizenclaw_curl_destroy(curl);

  if (http_code < 200 || http_code >= 300) {
    dlog_print(DLOG_WARNING, PROJECT_TAG, "HTTP %ld from MCP server",
               http_code);
  }

  dlog_print(DLOG_DEBUG, PROJECT_TAG, "MCP response (%ld): %.200s", http_code,
             response_buffer.c_str());

  return response_buffer;
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: mcp-http-bridge <url> [auth_token]" << std::endl;
    return 1;
  }

  std::string url = argv[1];
  std::string auth_token = (argc >= 3) ? argv[2] : "";

  dlog_print(DLOG_INFO, PROJECT_TAG, "MCP HTTP Bridge started: %s",
             url.c_str());

  // Main loop: read JSON-RPC from stdin, forward to HTTP, write to stdout
  std::string line;
  while (std::getline(std::cin, line)) {
    // Skip empty lines
    if (line.empty() || line == "\n" || line == "\r\n") {
      continue;
    }

    dlog_print(DLOG_DEBUG, PROJECT_TAG, "stdin → HTTP: %.200s", line.c_str());

    // Forward to HTTP MCP endpoint
    std::string response = SendHttpRequest(url, line, auth_token);

    // Write response to stdout for TizenClaw's McpClient to read
    if (!response.empty()) {
      std::cout << response << std::endl;
      std::cout.flush();
    }
  }

  dlog_print(DLOG_INFO, PROJECT_TAG, "MCP HTTP Bridge shutting down");

  return 0;
}
