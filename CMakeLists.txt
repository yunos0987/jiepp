cmake_minimum_required(VERSION 3.25)

if(WIN32 AND NOT JIEPP_OFFICIAL_WINDOWS_PRESET)
    message(WARNING "Windows builds should use the official preset. Configure with --preset windows-clang-ninja.")
endif()

# e.g. /home/user_name/jiepp/VERSION
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" JIEPP_VERSION)
string(STRIP "${JIEPP_VERSION}" JIEPP_VERSION)
project(jiepp VERSION ${JIEPP_VERSION} LANGUAGES CXX)

# ---- Sandbox mode for web deployment ----
option(JIEPP_SANDBOX "Enable sandbox mode: disable file-system directives and mask server paths" OFF)

# ---- Portable static binary (for deployment on older Linux servers) ----
# Produces a self-contained binary with no GLIBC/GLIBCXX version requirements.
# Use the linux-portable-release preset, which also sets VCPKG_TARGET_TRIPLET=x64-linux-static.
option(JIEPP_STATIC_LINK "Link all executables statically (-static)" OFF)
if(JIEPP_STATIC_LINK)
    add_link_options(-static)
endif()

# Debug: enable sanitizers for early bug detection (Linux/WSL only)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
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

BISON_TARGET(CfParser
    ${CMAKE_CURRENT_SOURCE_DIR}/src/constfold/constfold.y
    ${CMAKE_CURRENT_BINARY_DIR}/constfold_parser.cpp
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/constfold_parser.hpp
    COMPILE_FLAGS "-Wcounterexamples")

FLEX_TARGET(CfScanner
    ${CMAKE_CURRENT_SOURCE_DIR}/src/constfold/constfold.l
    ${CMAKE_CURRENT_BINARY_DIR}/constfold_scanner.cpp)

ADD_FLEX_BISON_DEPENDENCY(CfScanner CfParser)

# ---- Source files ----
set(JIEPP_SOURCES
    src/util/text.cpp
    src/util/path.cpp
    src/util/iec_61131-3.cpp
    src/loader/token.cpp
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
set_target_properties(jiepp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
target_link_libraries(jiepp PRIVATE jiepp_lib)

# ---- Tests ----
if(GTest_FOUND OR TARGET GTest::gtest)
    enable_testing()

    file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp" "tests/*/*.cpp" "tests/*/*/*.cpp")
    list(FILTER TEST_SOURCES EXCLUDE REGEX [[.*/debug_.*\.cpp$]])
    if(TEST_SOURCES)
        add_executable(jiepp_test ${TEST_SOURCES})
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
        )

        include(GoogleTest)
        gtest_discover_tests(jiepp_test)
    endif()
endif()

