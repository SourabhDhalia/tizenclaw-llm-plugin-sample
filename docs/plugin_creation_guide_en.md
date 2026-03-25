# TizenClaw LLM Plugin Creation Guide

This guide explains how to create a custom LLM plugin for the TizenClaw ecosystem. TizenClaw dynamically loads `.so` payload libraries that implement a specific C API contract, allowing users to swap out LLM backends easily.

## 1. Plugin Architecture Overview

TizenClaw interfaces with a backend plugin via a set of `extern "C"` functions defined in `<tizenclaw/llm-backend/tizenclaw_llm_backend.h>`. Any shared library functioning as a valid plugin must export these symbols without C++ name mangling.

TizenClaw will:
1. Load the shared library (`.so`).
2. Read the standard plugin configuration file.
3. Call `TIZENCLAW_LLM_BACKEND_INITIALIZE(config_json_str)`.
4. Forward user chat and tool requests to `TIZENCLAW_LLM_BACKEND_CHAT()`.
5. Call `TIZENCLAW_LLM_BACKEND_SHUTDOWN()` before unloading.

---

## 2. Implementing the Required Interfaces

Your backend code must implement the following core capabilities:

### Initialization and Shutdown

```cpp
#include <tizenclaw/llm-backend/tizenclaw_llm_backend.h>

extern "C" EXPORT bool TIZENCLAW_LLM_BACKEND_INITIALIZE(const char* config_json_str) {
    // Parse the incoming JSON config string (if provided)
    // Setup your internal contexts, memory structures, or API tokens
    return true; // Return false if initialization fails
}

extern "C" EXPORT const char* TIZENCLAW_LLM_BACKEND_GET_NAME(void) {
    // Return a static or stored C-style string representing your backend name.
    return "my-awesome-backend";
}

extern "C" EXPORT void TIZENCLAW_LLM_BACKEND_SHUTDOWN(void) {
    // Clean up allocated memory, API clients, and network sockets here.
}
```

### Handling Chat Completions

The heart of the plugin handles incoming message requests and tool definitions, formatting them for your target LLM (API or Local), and interpreting the result into a standardized `tizenclaw_llm_response_h` handle.

```cpp
extern "C" EXPORT tizenclaw_llm_response_h TIZENCLAW_LLM_BACKEND_CHAT(
    tizenclaw_llm_messages_h messages_arr, 
    tizenclaw_llm_tools_h tools_arr,
    tizenclaw_llm_backend_chunk_cb on_chunk, 
    void* user_data,
    const char* system_prompt) 
{
    tizenclaw_llm_response_h response = nullptr;
    tizenclaw_llm_response_create(&response);

    // 1. Iterate over the incoming messages
    tizenclaw_llm_messages_foreach(messages_arr, [](tizenclaw_llm_message_h msg, void* ud) -> bool {
        // Parse role, text, tool calls, and tool results
        return true;
    }, /* custom_user_data_ptr */);

    // 2. Iterate over the available tools (Function Calling)
    if (tools_arr) {
        tizenclaw_llm_tools_foreach(tools_arr, [](tizenclaw_llm_tool_h tool, void* ud) -> bool {
            // Read tool name, description, and JSON schema parameters
            return true;
        }, /* custom_user_data_ptr */);
    }

    // 3. Make Inference / Network Call
    // ...

    // 4. Populate Response Handle
    // Set text logic: tizenclaw_llm_response_set_text(response, "LLM Output");
    // Attach tool calls if any...
    
    // Set success flag so TizenClaw knows the inference was valid
    tizenclaw_llm_response_set_success(response, true);
    
    return response;
}
```

*Note: You are responsible for freeing strings returned by TizenClaw backend getter methods (e.g. `tizenclaw_llm_message_get_text`), as specified by the API headers.*

---

## 3. Network Interactions

If your LLM resides over a network API (such as OpenAI, Anthropic, or an inference server), you can use the natively bound `tizenclaw_curl_h` wrappers inside `tizenclaw_curl.h`. This ensures that your plugin uses the same optimized TLS and network layer libraries as the host daemon.

An example of firing a CURL request natively:

```cpp
#include <tizenclaw/llm-backend/tizenclaw_curl.h>

tizenclaw_curl_h curl = nullptr;
tizenclaw_curl_create(&curl);
tizenclaw_curl_set_url(curl, "https://api.openai.com/v1/chat/completions");
tizenclaw_curl_add_header(curl, "Content-Type: application/json");
tizenclaw_curl_set_post_data(curl, json_payload_string.c_str());

// Perform a blocking request (TizenClaw runs plugins asynchronously)
int res = tizenclaw_curl_perform(curl);
```

---

## 4. Building and Installation

### CMake Integration

To build your plugin, configure it as a shared library (`SHARED`) using CMake and statically link against `tizenclaw-core`. 

```cmake
PKG_CHECK_MODULES(PLUGIN_REQUIRED REQUIRED tizenclaw-core libcurl dlog)

ADD_LIBRARY(my-plugin SHARED src/my_plugin.cc)
TARGET_LINK_LIBRARIES(my-plugin PRIVATE ${PLUGIN_REQUIRED_LIBRARIES})
```

### unified-backend Registration

Once deployed onto the device, TizenClaw dynamically loads plugins using Tizen's application framework. Register your plugin using `unified-backend` during RPM installation deployment:

```bash
unified-backend --preload -y org.tizen.my-awesome-plugin
```

Restart `tizenclaw` to reflect changes:
```bash
systemctl restart tizenclaw
```
