cmake_minimum_required(VERSION 3.25)

if(WIN32 AND NOT JIEPP_OFFICIAL_WINDOWS_PRESET)
    message(WARNING "Windows builds should use the official preset. Configure with --preset windows-clang-ninja.")
endif()

# e.g. /home/user_name/jiepp/VERSION
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" JIEPP_VERSION)
string(STRIP "${JIEPP_VERSION}" JIEPP_VERSION)
project(jiepp VERSION ${JIEPP_VERSION} LANGUAGES CXX)

# Keep vcpkg.json's "version" field in sync with VERSION: vcpkg reads its own
# version from vcpkg.json, and the two drifting apart silently makes the
# vcpkg-reported package version wrong even though the built binary is correct.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json" _jiepp_vcpkg_json)
string(JSON _jiepp_vcpkg_version GET "${_jiepp_vcpkg_json}" version)
if(NOT _jiepp_vcpkg_version STREQUAL JIEPP_VERSION)
    message(FATAL_ERROR "vcpkg.json version (${_jiepp_vcpkg_version}) does not match VERSION (${JIEPP_VERSION}); bump both together")
endif()

# ---- Sandbox mode for web deployment ----
option(JIEPP_SANDBOX "Enable sandbox mode: disable file-system directives and blank __TIMESTAMP__ (file paths are not hidden)" OFF)

# ---- Portable static binary (for deployment on older Linux servers) ----
# Produces a self-contained binary with no GLIBC/GLIBCXX version requirements.
# Use the linux-portable-release preset. The default vcpkg x64-linux triplet
# already builds static libraries; -static additionally links glibc and
# libstdc++ statically.
option(JIEPP_STATIC_LINK "Link all executables statically (-static)" OFF)
if(JIEPP_STATIC_LINK)
    add_link_options(-static)
endif()

# Raise clang optimization for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-O3)
    # ThinLTO requires lld linker to avoid GNU ld/gold plugin version mismatches
    if(NOT WIN32)
        find_program(LLD_LINKER NAMES ld.lld lld)
        if(LLD_LINKER)
            add_compile_options(-flto=thin)
            add_link_options(-flto=thin -fuse-ld=lld)
            message(STATUS "Release build: ThinLTO enabled with lld linker")
        else()
            message(WARNING "Release build: lld linker not found. ThinLTO disabled to avoid GNU ld plugin mismatches. Install lld for ThinLTO support.")
        endif()
    else()
        # Windows: ThinLTO with lld-link (MSVC-compatible linker)
        add_compile_options(-flto=thin)
        add_link_options(-flto=thin)
    endif()
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Compiler / linker hardening ----
# jiepp may be fed untrusted input (see SECURITY.md), so every target built here
# (library, executable, tests) gets exploit mitigations:
#   Linux (gcc/clang): stack protector, _FORTIFY_SOURCE=2 (optimized configs
#     only: glibc rejects it at -O0), and full RELRO (-z relro -z now). PIE is
#     left at the toolchain default for dynamic builds; the fully static
#     linux-portable build stays non-PIE on purpose.
#   Windows (clang++ targeting MSVC, linked by lld-link): Control Flow Guard.
#     ASLR (/DYNAMICBASE, /HIGHENTROPYVA) and DEP (/NXCOMPAT) are lld-link
#     defaults and are not repeated here.
# The flags are added per target, never to the generated flex/bison sources'
# warning settings.
function(jiepp_harden_target target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_options(${target} PRIVATE
            -fstack-protector-strong
            "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:-U_FORTIFY_SOURCE>"
            "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:-D_FORTIFY_SOURCE=2>")
        get_target_property(_jiepp_type ${target} TYPE)
        if(NOT _jiepp_type STREQUAL "STATIC_LIBRARY")
            target_link_options(${target} PRIVATE "LINKER:-z,relro,-z,now")
        endif()
    elseif(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT MSVC)
        # clang++ (GNU-style driver) takes the CC1 flag through -Xclang;
        # clang-cl would take /guard:cf instead.
        target_compile_options(${target} PRIVATE "SHELL:-Xclang -cfguard")
        get_target_property(_jiepp_type ${target} TYPE)
        if(NOT _jiepp_type STREQUAL "STATIC_LIBRARY")
            target_link_options(${target} PRIVATE "LINKER:/guard:cf")
        endif()
    endif()
endfunction()

# vcpkg toolchain sets this up; if using vcpkg, GTest is found via find_package
find_package(GTest CONFIG QUIET)

# ---- Flex / Bison ----
if(WIN32)
    # Collect candidate search paths (Chocolatey + WinGet Links + WinGet Packages glob)
    set(_WIN_FLEX_HINTS
        "$ENV{ChocolateyInstall}/bin"
        "$ENV{ProgramData}/chocolatey/bin"
        "C:/ProgramData/chocolatey/bin"
        "$ENV{LOCALAPPDATA}/Microsoft/WinGet/Links"
    )
    # WinGet installs winflexbison to a package-ID-specific directory; glob for it
    file(GLOB _winget_flex_dirs
        "$ENV{LOCALAPPDATA}/Microsoft/WinGet/Packages/WinFlexBison*/")
    list(APPEND _WIN_FLEX_HINTS ${_winget_flex_dirs})

    if(NOT FLEX_EXECUTABLE)
        unset(FLEX_EXECUTABLE CACHE)
        find_program(FLEX_EXECUTABLE NAMES win_flex flex HINTS ${_WIN_FLEX_HINTS})
    endif()
    if(NOT BISON_EXECUTABLE)
        unset(BISON_EXECUTABLE CACHE)
        find_program(BISON_EXECUTABLE NAMES win_bison bison HINTS ${_WIN_FLEX_HINTS})
    endif()
endif()
find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

# ---- Python3 (for the perf test harness under bench/) ----
find_package(Python3 COMPONENTS Interpreter QUIET)

BISON_TARGET(CfParser
    ${CMAKE_CURRENT_SOURCE_DIR}/src/constfold/constfold.y
    ${CMAKE_CURRENT_BINARY_DIR}/constfold_parser.cpp
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/constfold_parser.hpp
    COMPILE_FLAGS "-Wcounterexamples")

FLEX_TARGET(CfScanner
    ${CMAKE_CURRENT_SOURCE_DIR}/src/constfold/constfold.l
    ${CMAKE_CURRENT_BINARY_DIR}/constfold_scanner.cpp)

ADD_FLEX_BISON_DEPENDENCY(CfScanner CfParser)

# The generated bison parser sets yynerrs_ but a non-debug build never reads
# it, which trips -Wunused-but-set-variable. Suppress that one warning only
# on the generated file, not project sources.
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/constfold_parser.cpp
    PROPERTIES COMPILE_OPTIONS -Wno-unused-but-set-variable)

# ---- Source files ----
set(JIEPP_SOURCES
    src/util/text.cpp
    src/util/path.cpp
    src/util/iec_61131-3.cpp
    src/util/stack_guard.cpp
    src/loader/token.cpp
    src/loader/hideset.cpp
    src/loader/directive_token.cpp
    src/loader/lexer.cpp
    src/loader/lexer_helpers.cpp
    src/loader/lexer_comment.cpp
    src/loader/lexer_pragma.cpp
    src/loader/lexer_literal.cpp
    src/loader/directive_parser.cpp
    src/core/directive_handlers.cpp
    src/core/expand.cpp
    src/core/expand_ctrl.cpp
    src/core/expand_subst.cpp
    src/core/line_compaction.cpp
    src/loader/loader.cpp
    src/env/env.cpp
    src/env/symtab.cpp
    src/env/file_context.cpp
    src/env/param.cpp
    src/env/issue.cpp
    src/env/issue_message.cpp
    src/macro/macro.cpp
    src/macro/macro_builtin.cpp
    src/constfold/constfold.cpp
    src/core/preprocessor.cpp
    src/jiepp/jiepp.cpp
    src/jiepp/option.cpp
    ${BISON_CfParser_OUTPUTS}
    ${FLEX_CfScanner_OUTPUTS}
)

add_library(jiepp_lib STATIC ${JIEPP_SOURCES})
target_compile_options(jiepp_lib PRIVATE -Wall -Wextra)
target_include_directories(jiepp_lib PUBLIC src ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(jiepp_lib PRIVATE
    JIEPP_VERSION="${PROJECT_VERSION}"
    JIEPP_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    JIEPP_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    JIEPP_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

if(JIEPP_SANDBOX)
    target_compile_definitions(jiepp_lib PUBLIC JIEPP_SANDBOX)
endif()

add_executable(jiepp src/jiepp/main.cpp)
target_compile_options(jiepp PRIVATE -Wall -Wextra)
set_target_properties(jiepp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
target_link_libraries(jiepp PRIVATE jiepp_lib)
jiepp_harden_target(jiepp_lib)
jiepp_harden_target(jiepp)

# ---- Tests ----
if(GTest_FOUND OR TARGET GTest::gtest)
    enable_testing()

    file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp" "tests/*/*.cpp" "tests/*/*/*.cpp")
    list(FILTER TEST_SOURCES EXCLUDE REGEX [[.*/debug_.*\.cpp$]])
    if(TEST_SOURCES)
        add_executable(jiepp_test ${TEST_SOURCES})
        target_compile_options(jiepp_test PRIVATE -Wall -Wextra)
        jiepp_harden_target(jiepp_test)
        # Disable linker pruning for test registration (MSVC only)
        if (MSVC)
            target_link_options(jiepp_test PRIVATE "/OPT:NOREF" "/INCREMENTAL:NO")
        endif()
        set_target_properties(jiepp_test PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        )
        target_link_libraries(jiepp_test PRIVATE jiepp_lib GTest::gtest_main)
        target_include_directories(jiepp_test PRIVATE tests)

        # Test data path
        target_compile_definitions(jiepp_test PRIVATE
            JIEPP_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
            JIEPP_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
            JIEPP_EXE_PATH="$<TARGET_FILE:jiepp>"
            # C8: tests/jiepp/test_jiepp_command.cpp's run_e2e() writes each
            # e2e case's actual .piec/.log/.d here instead of into
            # tests/jiepp/input, so the source tree stays clean after a test
            # run and a Debug and a Release ctest no longer race on the same
            # files (each build directory gets its own e2e-actual/).
            JIEPP_E2E_ACTUAL_DIR="${CMAKE_BINARY_DIR}/e2e-actual"
        )

        include(GoogleTest)
        gtest_discover_tests(jiepp_test)

        if(Python3_Interpreter_FOUND AND NOT JIEPP_SANDBOX)
            add_test(NAME perftest_smoke
                COMMAND ${Python3_EXECUTABLE}
                    "${CMAKE_CURRENT_SOURCE_DIR}/bench/perftest.py" selfcheck
                    --jiepp "$<TARGET_FILE:jiepp>"
                    --work "${CMAKE_BINARY_DIR}/perftest/selfcheck"
                    --build-type "$<CONFIG>"
            )
            set_tests_properties(perftest_smoke PROPERTIES LABELS "perf")
        endif()
    endif()
endif()

# ---- Benchmark (perf tests ported from jiecc, see bench/) ----
set(JIEPP_PERF_REPEAT "10" CACHE STRING "Number of repeated runs per perf test case")
set(JIEPP_PERF_MODE "all" CACHE STRING "Perf test case-size mode: all|greedy|greedy2")
set(JIEPP_PERF_RESULTS_DIR "${CMAKE_BINARY_DIR}/perftest/results" CACHE PATH "Directory for perf test TSV results")
set(JIEPP_PERF_CASES "" CACHE STRING "Semicolon-separated perf test case names to restrict to (default: empty = all cases)")

# Turn the JIEPP_PERF_CASES list (if any) into repeated --case NAME arguments.
set(_JIEPP_PERF_CASE_ARGS)
foreach(_jiepp_perf_case IN LISTS JIEPP_PERF_CASES)
    if(_jiepp_perf_case)
        list(APPEND _JIEPP_PERF_CASE_ARGS --case "${_jiepp_perf_case}")
    endif()
endforeach()

if(Python3_Interpreter_FOUND AND NOT JIEPP_SANDBOX)
    add_custom_target(perf_make
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/bench/perftest.py" make
            --cases-src "${CMAKE_CURRENT_SOURCE_DIR}/bench/cases"
            --out "${CMAKE_BINARY_DIR}/perftest/cases"
            --mode "${JIEPP_PERF_MODE}"
            ${_JIEPP_PERF_CASE_ARGS}
        COMMENT "Generating perf test cases"
        USES_TERMINAL
        VERBATIM
    )

    add_custom_target(bench
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/bench/perftest.py" run
            --jiepp "$<TARGET_FILE:jiepp>"
            --cases "${CMAKE_BINARY_DIR}/perftest/cases"
            --results "${JIEPP_PERF_RESULTS_DIR}"
            --repeat "${JIEPP_PERF_REPEAT}"
            --build-type "$<CONFIG>"
            ${_JIEPP_PERF_CASE_ARGS}
        COMMENT "Running perf tests"
        USES_TERMINAL
        VERBATIM
    )
    add_dependencies(bench jiepp perf_make)

    message(STATUS "Bench target enabled (Python3: ${Python3_EXECUTABLE})")
else()
    if(JIEPP_SANDBOX)
        set(_JIEPP_BENCH_DISABLED_REASON "JIEPP_SANDBOX is ON")
    else()
        set(_JIEPP_BENCH_DISABLED_REASON "Python 3 interpreter not found")
    endif()
    add_custom_target(bench
        COMMAND ${CMAKE_COMMAND} -E echo "bench target disabled: ${_JIEPP_BENCH_DISABLED_REASON}"
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "bench target disabled: ${_JIEPP_BENCH_DISABLED_REASON}"
        USES_TERMINAL
        VERBATIM
    )
    message(STATUS "Bench target disabled: ${_JIEPP_BENCH_DISABLED_REASON}")
endif()

