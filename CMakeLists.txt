#
# Copyright (c) 2026 Samsung Electronics Co., Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
PROJECT(tizenclaw-llm-plugin-sample LANGUAGES CXX)

## Compiler flags
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdata-sections")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmerge-all-constants")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

IF(NOT ASAN_ENABLED)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
ENDIF(NOT ASAN_ENABLED)

## Linker flags
SET(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed,--gc-sections -pie")

# Link against tizenclaw-llm-plugin for headers
INCLUDE(FindPkgConfig)

PKG_CHECK_MODULES(PLUGIN_REQUIRED REQUIRED
    tizenclaw-core
    libcurl
    dlog
)

ADD_LIBRARY(plugin-sample SHARED src/plugin-sample.cc)

TARGET_INCLUDE_DIRECTORIES(plugin-sample
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${PLUGIN_REQUIRED_INCLUDE_DIRS}
)

TARGET_LINK_LIBRARIES(plugin-sample PRIVATE ${PLUGIN_REQUIRED_LIBRARIES})

# MCP HTTP Bridge — stdio ↔ HTTP bridge for Swiggy MCP servers
ADD_EXECUTABLE(mcp-http-bridge src/mcp-http-bridge.cc)

TARGET_INCLUDE_DIRECTORIES(mcp-http-bridge
    PRIVATE
    ${PLUGIN_REQUIRED_INCLUDE_DIRS}
)

TARGET_LINK_LIBRARIES(mcp-http-bridge PRIVATE ${PLUGIN_REQUIRED_LIBRARIES})

# Install rules
# For Tizen RPK, files usually go into /opt/usr/globalapps/<pkgname>/res
SET(RPK_APP_DIR /usr/apps/org.tizen.tizenclaw-llm-plugin-sample)

INSTALL(TARGETS plugin-sample
    LIBRARY DESTINATION ${RPK_APP_DIR}/lib
)

INSTALL(TARGETS mcp-http-bridge
    RUNTIME DESTINATION ${RPK_APP_DIR}/bin
)

INSTALL(FILES
    res/plugin_llm_config.json
    res/mcp_servers_shopping.json
    res/agent_role_shopping.json
    res/autonomous_trigger_shopping.json
    DESTINATION ${RPK_APP_DIR}/res
)
