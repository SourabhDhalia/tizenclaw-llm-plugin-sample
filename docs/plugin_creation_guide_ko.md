# TizenClaw LLM 플러그인 제작 가이드

본 가이드는 TizenClaw 에코시스템에 맞게 사용자 정의 LLM 플러그인을 개발하는 방법을 설명합니다. TizenClaw는 규격화된 C API 조건에 맞는 특정 `.so` 공유 라이브러리를 동적으로 로드함으로써, 다양한 LLM 백엔드를 쉽게 스왑(Swap)하여 사용할 수 있게 해줍니다.

## 1. 개요 및 구조

TizenClaw 코어 아키텍처는 `<tizenclaw/llm-backend/tizenclaw_llm_backend.h>` 헤더 안에 정의된 약속된 `extern "C"` 함수들을 통해 백엔드 플러그인과 통신합니다. 커스텀 플러그인으로 동작하기 위해 구축된 공유 라이브러리는 반드시 C++의 Name Mangling이 발생하지 않고 이 C-level 심볼들을 외부에 노출해야 합니다.

TizenClaw 데몬의 라이프사이클은 다음과 같습니다:
1. 공유 라이브러리(`.so`) 동적 로드.
2. 시스템 플러그인 설정(JSON) 파일 판독.
3. `TIZENCLAW_LLM_BACKEND_INITIALIZE(config_json_str)` 최우선 호출.
4. 사용자 프롬프트 및 도구(Function calls) 배열을 가지고 `TIZENCLAW_LLM_BACKEND_CHAT()` 호출.
5. 시스템 종료 직전 자원 회수를 위해 `TIZENCLAW_LLM_BACKEND_SHUTDOWN()` 호출됨.

---

## 2. 필수 인터페이스 구현 방법

다음 핵심 로직에 대한 구현체를 작성해야 합니다:

### 초기화 및 종료

```cpp
#include <tizenclaw/llm-backend/tizenclaw_llm_backend.h>

extern "C" EXPORT bool TIZENCLAW_LLM_BACKEND_INITIALIZE(const char* config_json_str) {
    // 필요 시 들어오는 JSON 설정 데이터를 파싱
    // 내부 컨텍스트 생성이나 토큰 인증 과정 등을 수행
    return true; // 할당이나 인증에 실패했을 시 false 리턴
}

extern "C" EXPORT const char* TIZENCLAW_LLM_BACKEND_GET_NAME(void) {
    // 백엔드의 이름을 명시적으로 반환. C-style 정적 문자열 사용
    return "my-awesome-backend";
}

extern "C" EXPORT void TIZENCLAW_LLM_BACKEND_SHUTDOWN(void) {
    // 할당된 메모리 회수나 활성화된 네트워크 소켓 등의 자원 해제 로직 작성
}
```

### 채팅 결과 인터페이스 (주요 로직)

플러그인 동작의 핵심으로 송신된 메시지 배열과 도구를 백엔드(원격 API 혹은 로컬 NPU 런타임 등) 목적에 맞게 매핑하고, 결과를 `tizenclaw_llm_response_h` 구조의 핸들에 정리해 반환합니다.

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

    // 1. 전달받은 대화 히스토리 순회
    tizenclaw_llm_messages_foreach(messages_arr, [](tizenclaw_llm_message_h msg, void* ud) -> bool {
        // 역할(Role)이나 Text, 도구 호출명령들을 파싱 가능
        return true;
    }, /* custom_user_data_ptr */);

    // 2. 활용 기능 배열 (도구/Function Calling 리스트) 순회
    if (tools_arr) {
        tizenclaw_llm_tools_foreach(tools_arr, [](tizenclaw_llm_tool_h tool, void* ud) -> bool {
            // 도구명, 설명서, JSON 스키마를 C++ 구조체로 매핑
            return true;
        }, /* custom_user_data_ptr */);
    }

    // 3. 인퍼런스 또는 네트워크 호출 (cURL 혹은 Local LLM 프레임워크 호출)
    // ...

    // 4. TizenClaw에 전송할 정규화된 응답 핸들 구성
    // LLM 결과값을 Text 형태로 붙이는 예제: tizenclaw_llm_response_set_text(response, "LLM Output");
    // 도구 호출이 발생했을 시 tizenclaw_llm_response_add_llm_tool_call()를 통해 응답객체에 첨부
    
    // TizenClaw가 성공적인 결괏값인지 알도록 성공 플래그값 활성화
    tizenclaw_llm_response_set_success(response, true);
    
    return response;
}
```

*참고: `tizenclaw_llm_message_get_text` 등 Getter C API에서 반환받은 문자열은 메모리 누수 방지를 위해 명시적으로 `free()` 해 주어야 합니다.*

---

## 3. 네트워크 통신 구현하기 (옵션)

개발하는 LLM 솔루션이 원격 API 서버(OpenAI, Anthropic 등)를 매개로 한다면, Tizen 데몬의 네이티브 네트워크 동작과 TLS 모듈의 최적화를 위해 TizenClaw가 직접 래핑한 `tizenclaw_curl_h` 인터페이스(`<tizenclaw/llm-backend/tizenclaw_curl.h>`)를 사용할 것을 권장합니다.

네이티브 CURL 호출의 기본 구조:

```cpp
#include <tizenclaw/llm-backend/tizenclaw_curl.h>

tizenclaw_curl_h curl = nullptr;
tizenclaw_curl_create(&curl);
tizenclaw_curl_set_url(curl, "https://api.openai.com/v1/chat/completions");
tizenclaw_curl_add_header(curl, "Content-Type: application/json");
tizenclaw_curl_set_post_data(curl, json_payload_string.c_str());

// 동기식 네트워크 호출 방식. 데몬이 자체적으로 Async 스핀을 구성하므로 백엔드는 블로킹 요청으로 처리가 가능함.
int res = tizenclaw_curl_perform(curl);
```

---

## 4. 빌드 및 타겟 기기 설치

### CMake 환경 구성

빌드 시 `tizenclaw-core` 코어 개발의존성에 대해 `pkg-config` 링킹이 필요하며, 타겟 플랫폼에 대해 공유 라이브러리(`SHARED`)를 구축합니다. 

```cmake
PKG_CHECK_MODULES(PLUGIN_REQUIRED REQUIRED tizenclaw-core libcurl dlog)

ADD_LIBRARY(my-plugin SHARED src/my_plugin.cc)
TARGET_LINK_LIBRARIES(my-plugin PRIVATE ${PLUGIN_REQUIRED_LIBRARIES})
```

### unified-backend 모듈 등록

Tizen 기기로 배포가 이뤄지는 RPM 설치 환경 과정에서 Tizen 내장 앱 프레임워크가 플러그인을 인지하도록 등록해주는 과정이 필요합니다. `unified-backend` 명령어를 사용합니다.

```bash
unified-backend --preload -y org.tizen.my-awesome-plugin
```

상황에 맞게 등록이 완료된 이후라면 TizenClaw 프로세스를 재시작합니다:
```bash
systemctl restart tizenclaw
```
