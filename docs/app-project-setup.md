# External app repository setup

An app developed outside this monorepo must make the BSP contract available to
every contributor and every agent from the app repository root. Do not depend
on a person remembering to mention a sibling checkout in each new session.

## Required layout

Pin wilibsp in the app repository at `wilibsp/`, normally as a git submodule:

```bash
git submodule add https://github.com/freewili/wilibsp.git wilibsp
git -C wilibsp checkout <validated-wilibsp-commit>
git add .gitmodules wilibsp
```

Use a reviewed commit, not a floating branch. Initialize it in fresh checkouts
and CI with `git submodule update --init --recursive`.

In the app's top-level CMake, select the BSP board before importing the Pico
SDK, then add the BSP after `pico_sdk_init()`:

```cmake
set(PICO_BOARD freewili2 CACHE STRING "Board type")
list(APPEND PICO_BOARD_HEADER_DIRS
     "${CMAKE_CURRENT_LIST_DIR}/wilibsp/bsp/boards")
include(pico_sdk_import.cmake)
project(my_app C CXX ASM)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
pico_sdk_init()
add_subdirectory(wilibsp/bsp)

add_executable(my_app src/main.c)
target_link_libraries(my_app PRIVATE freewili2_bsp)
fw2_display_app(my_app
    VERSION 001
    DESCRIPTION "What the app visibly does"
    REPOSITORY "https://github.com/owner/my_app")
```

Do not add the wilibsp repository root with `add_subdirectory()`: that root
also registers all BSP example applications. External consumers add only
`wilibsp/bsp`.

The app repository must contain a short root `AGENTS.md`. Its opening lines
must tell an agent to read `wilibsp/AGENTS.md` completely before acting. Copy
[`templates/external-app-AGENTS.md`](templates/external-app-AGENTS.md); keep
app-specific rules below that mandatory pointer. If a tool truncates long
files, the instruction must explicitly require continuing in chunks until EOF.

If the repository supports Claude Code, its root `CLAUDE.md` must point to the
root `AGENTS.md`; copy
[`templates/external-app-CLAUDE.md`](templates/external-app-CLAUDE.md). Do not
duplicate the BSP rules in either file, because duplicated rules drift.

Validate the setup locally and in CI:

```bash
python wilibsp/tools/check_app_repo.py .
```

This check deliberately inspects only repository setup. Firmware builds still
enforce UF2 metadata, SRAM/PSRAM targets, HOME recovery, PAGE About behavior,
and the remaining app contract.
