cmake_minimum_required(VERSION 3.10)
project(happy-lines VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Sources ──────────────────────────────────────────────────────
set(SOURCES
    src/main.c
    src/platform.c
    src/argparser.c
    src/counter.c
    src/contributor.c
    src/hash_map.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PRIVATE ${CMAKE_SOURCE_DIR}/include
)

# ── Platform-specific linking ────────────────────────────────────
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
endif()

# ── Compiler options ─────────────────────────────────────────────
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /O2 /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -O2 -Wall -Wextra -Wpedantic)
endif()

# ── Install target ───────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
