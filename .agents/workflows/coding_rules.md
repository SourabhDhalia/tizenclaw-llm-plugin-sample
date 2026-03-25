---
description: TizenClaw Coding Rules and Guidelines
---

# TizenClaw Agent Support Rules

When implementing TizenClaw in this repository, the Agent (AI) must always prioritize and adhere to the following coding styles and rules.

## 1. C++ Coding Style
- **C++ Standard**: Use **C++20** (`-std=c++20`).
- **Style Guide**: Strictly follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
- **Line Wrap**: Ensure all text in source code, comments, and header files is appropriately wrapped **not to exceed 80 characters (Column limit: 80)**.
- **Indentation**: Use 2 spaces (Space 2). Do not use tabs.
- **Naming Conventions**:
  - Class/Struct: PascalCase (e.g., `AgentCore`, `SandboxManager`)
  - Variables: snake_case (e.g., `app_data`, `cmd_line`)
  - Member Variables: Uniformly apply an `m_` prefix or `_` suffix (e.g., `m_initialized` or `initialized_`).
  - Functions: PascalCase, or snake_case allowed when wrapping Tizen C APIs.
- **C++20 Mandatory Rules**:
  - `[[nodiscard]]`: Apply to bool/state returning functions.
  - `std::filesystem`: Use instead of POSIX `opendir/readdir/stat`.
  - `map::contains()`: Use instead of `find() != end()`.
  - `std::ranges`: Prioritize range-based algorithms.
  - `using enum`: Apply for repeated enumeration use within scope.

## 2. CMake and Build Support
- Written targeting the Tizen GBS (Gerrit Build System) environment, `gbs build` must always succeed via CMake.
- When adding new C++ source files, you must update the `SOURCES` list in `CMakeLists.txt`.

## 3. Tizen-Specific Rules
- Features requiring privileges (Network, LXC execution, AppManager, etc.) must be explicitly stated in the `<privileges>` block of `tizen-manifest.xml`.
- Make full use of the dlog interface (`dlog_print`) to leave comprehensive system logs, and prioritize error handling via return codes or boolean returns over C++ exceptions whenever possible.
