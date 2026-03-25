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

#include <dlog.h>
#include <tizenclaw/llm-backend/tizenclaw_curl.h>
#include <tizenclaw/llm-backend/tizenclaw_llm_backend.h>
#include <unistd.h>

#include <iostream>
#include <string>

#undef PROJECT_TAG
#define PROJECT_TAG "TIZENCLAW_LLM_PLUGIN"

#include "nlohmann/json.hpp"

#undef EXPORT
#define EXPORT __attribute__((visibility("default")))

using json = nlohmann::json;

namespace {

struct OpenAIBackend {
  std::string name;
  std::string api_key;
  std::string model;
  std::string endpoint;
};

OpenAIBackend* g_backend = nullptr;

struct PluginWriteContext {
  std::string* buffer;
};

void PluginWriteCallback(const char* chunk, void* user_data) {
  auto* ctx = static_cast<PluginWriteContext*>(user_data);
  if (chunk && ctx->buffer) {
    ctx->buffer->append(chunk);
  }
}

}  // namespace

extern "C" EXPORT bool TIZENCLAW_LLM_BACKEND_INITIALIZE(const char* config_json_str) {
  if (g_backend) return true;

  g_backend = new (std::nothrow) OpenAIBackend();
  if (!g_backend) return false;

  g_backend->name = "plugin-sample";
  g_backend->model = "gpt-3.5-turbo";
  g_backend->endpoint = "https://api.openai.com/v1/chat/completions";

  if (config_json_str) {
    try {
      auto config = json::parse(config_json_str);
      if (config.contains("name")) g_backend->name = config["name"];
      if (config.contains("api_key")) g_backend->api_key = config["api_key"];
      if (config.contains("model")) g_backend->model = config["model"];
      if (config.contains("endpoint")) g_backend->endpoint = config["endpoint"];
    } catch (...) {
      dlog_print(DLOG_ERROR, PROJECT_TAG, "[Plugin Sample] Failed to parse config JSON");
    }
  }

  if (g_backend->api_key.empty()) {
    const char* env_key = getenv("OPENAI_API_KEY");
    if (env_key) g_backend->api_key = env_key;
  }
  dlog_print(DLOG_INFO, PROJECT_TAG, "[Plugin Sample] Initialized (model: %s, endpoint: %s)",
             g_backend->model.c_str(), g_backend->endpoint.c_str());
  return true;
}

extern "C" EXPORT const char* TIZENCLAW_LLM_BACKEND_GET_NAME(void) {
  return g_backend ? g_backend->name.c_str() : "unknown";
}

extern "C" EXPORT void TIZENCLAW_LLM_BACKEND_SHUTDOWN(void) {
  if (g_backend) {
    dlog_print(DLOG_INFO, PROJECT_TAG, "[Plugin Sample] Shutdown");
    delete g_backend;
    g_backend = nullptr;
  }
}

extern "C" EXPORT tizenclaw_llm_response_h TIZENCLAW_LLM_BACKEND_CHAT(
    tizenclaw_llm_messages_h messages_arr, tizenclaw_llm_tools_h tools_arr,
    tizenclaw_llm_backend_chunk_cb on_chunk, void* user_data,
    const char* system_prompt) {
  tizenclaw_llm_response_h response = nullptr;
  tizenclaw_llm_response_create(&response);

  if (!g_backend) {
    tizenclaw_llm_response_set_error_message(response,
                                             "Plugin not initialized");
    return response;
  }

  json payload;
  payload["model"] = g_backend->model;

  json req_messages = json::array();
  if (system_prompt && strlen(system_prompt) > 0) {
    req_messages.push_back({{"role", "system"}, {"content", system_prompt}});
  }

  if (messages_arr) {
    auto msg_cb = [](tizenclaw_llm_message_h msg, void* ud) -> bool {
      auto* req_msgs = static_cast<json*>(ud);
      json m;
      char* role = nullptr;
      tizenclaw_llm_message_get_role(msg, &role);
      if (role) {
        m["role"] = role;
        free(role);
      }

      char* text = nullptr;
      tizenclaw_llm_message_get_text(msg, &text);
      if (text) {
        m["content"] = text;
        free(text);
      }

      // Tool calls
      json tool_calls = json::array();
      auto tc_cb = [](tizenclaw_llm_tool_call_h tc, void* u) -> bool {
        auto* tcs = static_cast<json*>(u);
        json c;
        char* id = nullptr;
        tizenclaw_llm_tool_call_get_id(tc, &id);
        if (id) {
          c["id"] = id;
          free(id);
        }

        char* name = nullptr;
        tizenclaw_llm_tool_call_get_name(tc, &name);
        if (name) {
          c["type"] = "function";
          c["function"]["name"] = name;
          free(name);
        }

        char* args = nullptr;
        tizenclaw_llm_tool_call_get_args_json(tc, &args);
        if (args) {
          c["function"]["arguments"] = args;
          free(args);
        }

        tcs->push_back(c);
        return true;
      };
      tizenclaw_llm_message_foreach_tool_calls(msg, tc_cb, &tool_calls);
      if (!tool_calls.empty()) m["tool_calls"] = tool_calls;

      // Handle tool results
      char* tool_name = nullptr;
      tizenclaw_llm_message_get_tool_name(msg, &tool_name);
      if (tool_name) {
        m["name"] = tool_name;
        free(tool_name);

        char* tc_id = nullptr;
        tizenclaw_llm_message_get_tool_call_id(msg, &tc_id);
        if (tc_id) {
          m["tool_call_id"] = tc_id;
          free(tc_id);
        }

        char* res = nullptr;
        tizenclaw_llm_message_get_tool_result_json(msg, &res);
        if (res) {
          m["content"] = res;
          free(res);
        }  // OpenAI expects tool result in content
      }

      req_msgs->push_back(m);
      return true;
    };
    tizenclaw_llm_messages_foreach(messages_arr, msg_cb, &req_messages);
  }
  payload["messages"] = req_messages;

  if (tools_arr) {
    json req_tools = json::array();
    auto tool_cb = [](tizenclaw_llm_tool_h tool, void* ud) -> bool {
      auto* rt = static_cast<json*>(ud);
      json t;
      t["type"] = "function";
      json func;

      char* name = nullptr;
      tizenclaw_llm_tool_get_name(tool, &name);
      if (name) {
        func["name"] = name;
        free(name);
      }

      char* desc = nullptr;
      tizenclaw_llm_tool_get_description(tool, &desc);
      if (desc) {
        func["description"] = desc;
        free(desc);
      }

      char* params = nullptr;
      tizenclaw_llm_tool_get_parameters_json(tool, &params);
      if (params) {
        try {
          func["parameters"] = json::parse(params);
        } catch (...) {
        }
        free(params);
      }

      t["function"] = func;
      rt->push_back(t);
      return true;
    };
    tizenclaw_llm_tools_foreach(tools_arr, tool_cb, &req_tools);
    if (!req_tools.empty()) payload["tools"] = req_tools;
  }

  std::string payload_str = payload.dump();

  tizenclaw_curl_h curl = nullptr;
  if (tizenclaw_curl_create(&curl) == TIZENCLAW_ERROR_NONE) {
    tizenclaw_curl_set_url(curl, g_backend->endpoint.c_str());

    tizenclaw_curl_add_header(curl, "Content-Type: application/json");
    std::string auth_header = "Authorization: Bearer " + g_backend->api_key;
    tizenclaw_curl_add_header(curl, auth_header.c_str());

    tizenclaw_curl_set_post_data(curl, payload_str.c_str());

    std::string readBuffer;
    PluginWriteContext write_ctx;
    write_ctx.buffer = &readBuffer;
    tizenclaw_curl_set_write_callback(curl, PluginWriteCallback, &write_ctx);
    tizenclaw_curl_set_timeout(curl, 10, 60);

    int res = tizenclaw_curl_perform(curl);
    if (res == TIZENCLAW_ERROR_NONE) {
      long http_code = 0;
      tizenclaw_curl_get_response_code(curl, &http_code);
      tizenclaw_llm_response_set_http_status(response, http_code);

      dlog_print(DLOG_DEBUG, PROJECT_TAG, "[Plugin Sample] Raw response: %s", readBuffer.c_str());

      try {
        auto resp_json = json::parse(readBuffer);
        if (resp_json.contains("choices") && resp_json["choices"].size() > 0) {
          auto message = resp_json["choices"][0]["message"];
          if (message.contains("content") && !message["content"].is_null()) {
            tizenclaw_llm_response_set_text(
                response, message["content"].get<std::string>().c_str());
          }
          if (message.contains("tool_calls")) {
            for (const auto& tc_json : message["tool_calls"]) {
              tizenclaw_llm_tool_call_h tc_h = nullptr;
              tizenclaw_llm_tool_call_create(&tc_h);
              if (tc_json.contains("id")) {
                tizenclaw_llm_tool_call_set_id(
                    tc_h, tc_json["id"].get<std::string>().c_str());
              }
              if (tc_json.contains("function")) {
                auto fn = tc_json["function"];
                if (fn.contains("name"))
                  tizenclaw_llm_tool_call_set_name(
                      tc_h, fn["name"].get<std::string>().c_str());
                if (fn.contains("arguments"))
                  tizenclaw_llm_tool_call_set_args_json(
                      tc_h, fn["arguments"].get<std::string>().c_str());
              }
              tizenclaw_llm_response_add_llm_tool_call(response, tc_h);
              tizenclaw_llm_tool_call_destroy(tc_h);
            }
          }
          tizenclaw_llm_response_set_success(response, true);
        } else if (resp_json.contains("error")) {
          tizenclaw_llm_response_set_error_message(
              response, resp_json["error"].dump().c_str());
        }

        if (resp_json.contains("usage")) {
          auto usage = resp_json["usage"];
          if (usage.contains("prompt_tokens"))
            tizenclaw_llm_response_set_prompt_tokens(
                response, usage["prompt_tokens"].get<int>());
          if (usage.contains("completion_tokens"))
            tizenclaw_llm_response_set_completion_tokens(
                response, usage["completion_tokens"].get<int>());
          if (usage.contains("total_tokens"))
            tizenclaw_llm_response_set_total_tokens(
                response, usage["total_tokens"].get<int>());
        }

      } catch (...) {
        tizenclaw_llm_response_set_error_message(
            response, "Failed to parse OpenAI response");
      }
    } else {
      const char* err = tizenclaw_curl_get_error_message(curl);
      std::string err_str =
          std::string("CURL request failed: ") + (err ? err : "Unknown error");
      tizenclaw_llm_response_set_error_message(response, err_str.c_str());
    }
    tizenclaw_curl_destroy(curl);
  }

  return response;
}
