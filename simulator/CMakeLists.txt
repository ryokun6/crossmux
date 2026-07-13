cmake_minimum_required(VERSION 3.16)
project(crosspoint_simulator LANGUAGES CXX C)

get_filename_component(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)

# -----------------------------------------------------------------------------
# WASM uses pthreads (Web Workers + SharedArrayBuffer) to back FreeRTOS-on-std::thread.
# Shared-memory linking requires EVERY object — including the arduino-host subdirectory
# and the vendored C libs — to be built with atomics/bulk-memory (-pthread). Set it at
# directory scope BEFORE add_subdirectory so it propagates to all targets below.
if(EMSCRIPTEN)
    add_compile_options(-pthread)
    add_link_options(-pthread)
endif()

# -----------------------------------------------------------------------------
add_subdirectory(arduino-host)

# Third-party: ArduinoJson (header-only, fetched once).
include(FetchContent)
FetchContent_Declare(
    arduinojson
    GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
    GIT_TAG v7.4.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(arduinojson)

# Third-party: ricmoo/QRCode (the same lib the firmware pulls via lib_deps). Plain
# Arduino C lib with no CMakeLists, so populate then compile qrcode.c ourselves.
# Lets the simulator render the real AirPage QR instead of the no-op shim. Fetched
# for both native and WASM (qrcode_SOURCE_DIR feeds VENDOR_C + include dirs below).
FetchContent_Declare(
    qrcode
    GIT_REPOSITORY https://github.com/ricmoo/QRCode.git
    GIT_TAG v0.0.1
    GIT_SHALLOW TRUE
)
# QRCode ships no CMakeLists, so MakeAvailable just populates (no add_subdirectory)
# and still sets qrcode_SOURCE_DIR. Replaces the deprecated FetchContent_Populate().
FetchContent_MakeAvailable(qrcode)

# bitbank2/JPEGDEC — same pin as platformio.ini. Needed for home/recent cover
# thumbnails (JpegToBmpConverter). WASM keeps the stub path (no FetchContent).
if(NOT EMSCRIPTEN)
    set(JPEGDEC_ROOT "")
    foreach(_jpegdec_candidate
            "${REPO_ROOT}/.pio/libdeps/default/JPEGDEC"
            "${REPO_ROOT}/.pio/libdeps/gh_release/JPEGDEC"
            "${REPO_ROOT}/.pio/libdeps/gh_release_tc/JPEGDEC"
            "${REPO_ROOT}/.pio/libdeps/gh_release_sc/JPEGDEC")
        if(EXISTS "${_jpegdec_candidate}/src/JPEGDEC.cpp")
            set(JPEGDEC_ROOT "${_jpegdec_candidate}")
            message(STATUS "JPEGDEC: using PlatformIO libdep at ${JPEGDEC_ROOT}")
            break()
        endif()
    endforeach()
    if(JPEGDEC_ROOT STREQUAL "")
        # JPEGDEC's tree has an ESP-IDF CMakeLists; populate without add_subdirectory.
        FetchContent_Declare(
            jpegdec
            GIT_REPOSITORY https://github.com/bitbank2/JPEGDEC.git
            GIT_TAG 86282979224c8a32fd51e091ed5a35b0c699a52b
        )
        FetchContent_GetProperties(jpegdec)
        if(NOT jpegdec_POPULATED)
            FetchContent_Populate(jpegdec)
        endif()
        set(JPEGDEC_ROOT "${jpegdec_SOURCE_DIR}")
        message(STATUS "JPEGDEC: fetched to ${JPEGDEC_ROOT}")
    endif()
    # CrossPoint's progressive-JPEG MCU patches (scripts/patch_jpegdec.py equivalent).
    file(GLOB JPEGDEC_PATCHES "${REPO_ROOT}/scripts/jpegdec_patches/*.patch")
    list(SORT JPEGDEC_PATCHES)
    foreach(patch ${JPEGDEC_PATCHES})
        execute_process(
            COMMAND git apply --check --reverse "${patch}"
            WORKING_DIRECTORY "${JPEGDEC_ROOT}"
            RESULT_VARIABLE JPEGDEC_PATCH_ALREADY
            OUTPUT_QUIET ERROR_QUIET
        )
        if(NOT JPEGDEC_PATCH_ALREADY EQUAL 0)
            execute_process(
                COMMAND git apply "${patch}"
                WORKING_DIRECTORY "${JPEGDEC_ROOT}"
                RESULT_VARIABLE JPEGDEC_PATCH_OK
                OUTPUT_QUIET ERROR_QUIET
            )
            if(NOT JPEGDEC_PATCH_OK EQUAL 0)
                message(WARNING "JPEGDEC patch failed to apply: ${patch}")
            else()
                message(STATUS "Applied JPEGDEC patch: ${patch}")
            endif()
        endif()
    endforeach()
endif()

# libcurl — backs the native simulator's esp_http_client shim (shims/esp_http_client.h)
# so WeRead, AirPage, and any other on-device HTTPS code paths we don't exclude can hit
# the real network from host. The WASM build is offline-only (the same shim compiles a
# curl-free stub under #ifdef __EMSCRIPTEN__), so it skips libcurl entirely.
if(NOT EMSCRIPTEN)
    find_package(CURL REQUIRED)
endif()

# SDL2 — native simulator window only. The WASM build renders via the browser canvas
# and reads input from JavaScript, so it has no SDL dependency.
if(NOT EMSCRIPTEN)
    find_package(SDL2 QUIET)
    if(NOT SDL2_FOUND)
        find_program(SDL2_CONFIG NAMES sdl2-config)
        if(SDL2_CONFIG)
            execute_process(COMMAND ${SDL2_CONFIG} --cflags
                OUTPUT_VARIABLE SDL2_CFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
            execute_process(COMMAND ${SDL2_CONFIG} --libs
                OUTPUT_VARIABLE SDL2_LDFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
            separate_arguments(SDL2_CFLAGS UNIX_COMMAND "${SDL2_CFLAGS}")
            separate_arguments(SDL2_LDFLAGS UNIX_COMMAND "${SDL2_LDFLAGS}")
        else()
            message(FATAL_ERROR "SDL2 not found. Install via 'brew install sdl2' or 'apt install libsdl2-dev'.")
        endif()
    endif()
endif()

# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# Project source globs
# -----------------------------------------------------------------------------
file(GLOB_RECURSE CROSSPOINT_SRC "${REPO_ROOT}/src/*.cpp")
file(GLOB_RECURSE CROSSPOINT_LIB
    "${REPO_ROOT}/lib/Epub/*.cpp"
    "${REPO_ROOT}/lib/EpdFont/*.cpp"
    "${REPO_ROOT}/lib/FsHelpers/*.cpp"
    "${REPO_ROOT}/lib/GfxRenderer/*.cpp"
    "${REPO_ROOT}/lib/I18n/*.cpp"
    "${REPO_ROOT}/lib/InflateReader/*.cpp"
    "${REPO_ROOT}/lib/JpegToBmpConverter/*.cpp"
    "${REPO_ROOT}/lib/JsonParser/*.cpp"
    "${REPO_ROOT}/lib/Logging/*.cpp"
    "${REPO_ROOT}/lib/MiniBidi/*.cpp"
    "${REPO_ROOT}/lib/OpdsParser/*.cpp"
    "${REPO_ROOT}/lib/PngToBmpConverter/*.cpp"
    "${REPO_ROOT}/lib/Serialization/*.cpp"
    "${REPO_ROOT}/lib/Txt/*.cpp"
    "${REPO_ROOT}/lib/Utf8/*.cpp"
    "${REPO_ROOT}/lib/XmlParserUtils/*.cpp"
    "${REPO_ROOT}/lib/Xtc/*.cpp"
    "${REPO_ROOT}/lib/ZipFile/*.cpp"
)

# Real HAL .cpp is replaced by simulator/hal/.
list(FILTER CROSSPOINT_LIB EXCLUDE REGEX "lib/hal/Hal.*\\.cpp$")

# Network / OTA .cpp files use APIs that are too deep to shim for first-version
# simulator scope. Image converters (JPEG/PNG → BMP) are enabled on native so
# home/recent book covers work; WASM keeps stubs. Inline EPUB image decoders
# stay excluded on both (heavier; covers don't need them).

list(FILTER CROSSPOINT_SRC EXCLUDE REGEX "src/network/")
# ...but keep the real HttpDownloader: the libcurl-backed esp_http_client shim lets it
# actually fetch over the network, so AirPage and WeRead can hit the real internet.
list(APPEND CROSSPOINT_SRC ${REPO_ROOT}/src/network/HttpDownloader.cpp)
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX "src/activities/network/")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX "OpdsBookBrowserActivity")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX "OtaUpdateActivity")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX "SdFirmwareUpdateActivity")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX "FontDownloadActivity")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX "KOReaderAuthActivity")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX "KOReaderSettingsActivity")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX "KOReaderSyncActivity")

if(EMSCRIPTEN)
    list(FILTER CROSSPOINT_LIB EXCLUDE REGEX "lib/JpegToBmpConverter/")
    list(FILTER CROSSPOINT_LIB EXCLUDE REGEX "lib/PngToBmpConverter/")
endif()
list(FILTER CROSSPOINT_LIB EXCLUDE REGEX "lib/Epub/Epub/converters/")
list(FILTER CROSSPOINT_LIB EXCLUDE REGEX "lib/Serialization/ObfuscationUtils")

# KOReaderSync credential store referenced via the KOREADER_STORE singleton in main.cpp.
# KOReaderJsonIO.cpp implements the load/save backing the store and must link with it.
# KOReaderDocumentId.cpp backs BookIdentity::calculateContentBookId (reading-stats book
# identity); it only depends on HalStorage/Logging/MD5Builder, all available on host.
# ProgressMapper.cpp + ChapterXPathResolver.cpp map saved XPath/percent positions to
# CrossPoint pages — now used by the bookmarks UI (not just KOReader sync). Their deps
# (GfxRenderer/Epub/Utf8/XmlParserUtils/expat) are all host-available, so compile them for
# real instead of stubbing, otherwise bookmark navigation returns empty positions.
list(APPEND CROSSPOINT_LIB
    ${REPO_ROOT}/lib/KOReaderSync/KOReaderCredentialStore.cpp
    ${REPO_ROOT}/lib/KOReaderSync/KOReaderJsonIO.cpp
    ${REPO_ROOT}/lib/KOReaderSync/KOReaderDocumentId.cpp
    ${REPO_ROOT}/lib/KOReaderSync/ChapterXPathResolver.cpp
    ${REPO_ROOT}/lib/KOReaderSync/ProgressMapper.cpp
)

# HAL backends for the simulator. All implementations live in hal/ and compile for both the
# native (SDL) and WASM (browser) builds. HalDisplay/HalGPIO carry the only platform split,
# behind #ifdef __EMSCRIPTEN__ inside the file: native pushes the framebuffer to an SDL texture
# and reads SDL events; WASM flips a framebuffer dirty-flag and takes input from JavaScript.
# Clock/Power/Storage/System/Tilt are pure POSIX + std::chrono, which Emscripten supports as-is.
set(SIMULATOR_HAL
    hal/HalClock.cpp
    hal/HalDisplay.cpp
    hal/HalGPIO.cpp
    hal/HalPowerManager.cpp
    hal/HalStorage.cpp
    hal/HalSystem.cpp
    hal/HalTiltSensor.cpp
)

# Vendored third-party C libraries inside the repo (+ the fetched QRCode lib).
set(VENDOR_C
    ${REPO_ROOT}/lib/expat/xmlparse.c
    ${REPO_ROOT}/lib/expat/xmlrole.c
    ${REPO_ROOT}/lib/expat/xmltok.c
    ${REPO_ROOT}/lib/uzlib/src/tinflate.c
    ${REPO_ROOT}/lib/MiniBidi/minibidi.c
    ${qrcode_SOURCE_DIR}/src/qrcode.c
)

# Host JPEGDEC (C++). ESP32 SIMD .S files are omitted — scalar decode is fine on desktop.
if(NOT EMSCRIPTEN)
    list(APPEND CROSSPOINT_LIB ${JPEGDEC_ROOT}/src/JPEGDEC.cpp)
endif()

# CJK simulator SKU selection (mutually exclusive):
#   default              → Traditional Chinese (gh_release_tc)
#   SIMULATOR_CHINESE_UI_SIMPLIFIED=ON → Simplified Chinese (gh_release_sc)
#   SIMULATOR_JAPANESE_VERSION=ON     → Japanese (gh_release_ja)
#   SIMULATOR_KOREAN_VERSION=ON       → Korean (gh_release_ko)
# Use separate build dirs per SKU — gen_i18n writes shared lib/I18n/ headers.
option(SIMULATOR_CHINESE_UI_SIMPLIFIED "Build Simplified Chinese simulator SKU" OFF)
option(SIMULATOR_JAPANESE_VERSION "Build Japanese simulator SKU" OFF)
option(SIMULATOR_KOREAN_VERSION "Build Korean simulator SKU" OFF)

set(_SIM_CJK_SKU_COUNT 0)
if(SIMULATOR_CHINESE_UI_SIMPLIFIED)
    math(EXPR _SIM_CJK_SKU_COUNT "${_SIM_CJK_SKU_COUNT}+1")
endif()
if(SIMULATOR_JAPANESE_VERSION)
    math(EXPR _SIM_CJK_SKU_COUNT "${_SIM_CJK_SKU_COUNT}+1")
endif()
if(SIMULATOR_KOREAN_VERSION)
    math(EXPR _SIM_CJK_SKU_COUNT "${_SIM_CJK_SKU_COUNT}+1")
endif()
if(_SIM_CJK_SKU_COUNT GREATER 1)
    message(FATAL_ERROR
        "Pick at most one of SIMULATOR_CHINESE_UI_SIMPLIFIED / "
        "SIMULATOR_JAPANESE_VERSION / SIMULATOR_KOREAN_VERSION")
endif()

# Prefer repo/.venv (has OpenCC for SC tw2sp). Override with -DI18N_PYTHON=...
if(NOT I18N_PYTHON)
    if(EXISTS "${REPO_ROOT}/.venv/bin/python3")
        set(I18N_PYTHON "${REPO_ROOT}/.venv/bin/python3")
    elseif(DEFINED ENV{VIRTUAL_ENV} AND EXISTS "$ENV{VIRTUAL_ENV}/bin/python3")
        set(I18N_PYTHON "$ENV{VIRTUAL_ENV}/bin/python3")
    else()
        find_program(I18N_PYTHON NAMES python3 python REQUIRED)
    endif()
endif()
if(SIMULATOR_CHINESE_UI_SIMPLIFIED)
    execute_process(
        COMMAND "${I18N_PYTHON}" -c "from opencc import OpenCC"
        RESULT_VARIABLE _I18N_OPENCC_OK
        OUTPUT_QUIET ERROR_QUIET)
    if(NOT _I18N_OPENCC_OK EQUAL 0)
        message(FATAL_ERROR
            "SC simulator (SIMULATOR_CHINESE_UI_SIMPLIFIED=ON) needs OpenCC in:\n"
            "  ${I18N_PYTHON}\n"
            "Create/use the repo venv:\n"
            "  python3 -m venv .venv && .venv/bin/pip install OpenCC\n"
            "Or point CMake at a Python that has it:\n"
            "  cmake -DI18N_PYTHON=/path/to/venv/bin/python3 ...")
    endif()
endif()

# Generate I18n headers (mirrors what scripts/gen_i18n.py does pre-build for PlatformIO).
# All translation YAMLs are listed as inputs so edits to any one of them re-run the
# generator; otherwise downstream code referencing a freshly-added STR_* key would
# fail to compile after a merge that only touched the YAMLs.
set(I18N_GENERATED
    ${REPO_ROOT}/lib/I18n/I18nKeys.h
    ${REPO_ROOT}/lib/I18n/I18nStrings.h
    ${REPO_ROOT}/lib/I18n/I18nStrings.cpp
)
file(GLOB I18N_TRANSLATIONS "${REPO_ROOT}/lib/I18n/translations/*.yaml")
if(SIMULATOR_CHINESE_UI_SIMPLIFIED)
    set(I18N_GEN_ARGS
        ${REPO_ROOT}/lib/I18n/translations ${REPO_ROOT}/lib/I18n
        --only-languages ZH_CN)
    set(I18N_GEN_COMMENT "Generating I18n strings (EN + ZH_CN)")
elseif(SIMULATOR_JAPANESE_VERSION)
    set(I18N_GEN_ARGS
        ${REPO_ROOT}/lib/I18n/translations ${REPO_ROOT}/lib/I18n
        --only-languages JA)
    set(I18N_GEN_COMMENT "Generating I18n strings (EN + JA)")
elseif(SIMULATOR_KOREAN_VERSION)
    set(I18N_GEN_ARGS
        ${REPO_ROOT}/lib/I18n/translations ${REPO_ROOT}/lib/I18n
        --only-languages KO)
    set(I18N_GEN_COMMENT "Generating I18n strings (EN + KO)")
else()
    # Default simulator keeps the full language table (WASM language picker) and
    # Traditional chinese.yaml as ZH_TW — same as a TC-capable Chinese demo.
    set(I18N_GEN_ARGS ${REPO_ROOT}/lib/I18n/translations ${REPO_ROOT}/lib/I18n)
    set(I18N_GEN_COMMENT "Generating I18n strings")
endif()
add_custom_command(
    OUTPUT ${I18N_GENERATED}
    COMMAND "${I18N_PYTHON}" ${REPO_ROOT}/scripts/gen_i18n.py ${I18N_GEN_ARGS}
    DEPENDS ${REPO_ROOT}/scripts/gen_i18n.py ${I18N_TRANSLATIONS}
    COMMENT ${I18N_GEN_COMMENT}
)
add_custom_target(i18n_generated DEPENDS ${I18N_GENERATED})

# WeRead is Chinese-SKU only (mirrors PlatformIO build_src_filter). Exclude
# before add_executable so JA/KO builds don't reference gated ActivityManager APIs.
if(SIMULATOR_JAPANESE_VERSION OR SIMULATOR_KOREAN_VERSION)
    list(FILTER CROSSPOINT_SRC EXCLUDE REGEX "activities/apps/weread/")
endif()

# Entry point + windowing. Native drives an SDL window/event loop; WASM has no window
# (rendering + input are owned by the browser), so it uses a minimal entry point.
if(EMSCRIPTEN)
    set(SIMULATOR_ENTRY simulator_main_wasm.cpp)
else()
    set(SIMULATOR_ENTRY
        simulator_main.cpp
        simulator_window.cpp
        simulator_shell.cpp
        simulator_overlay.cpp
        simulator_settings.cpp)
endif()

add_executable(crosspoint_simulator
    ${SIMULATOR_ENTRY}
    missing_symbols.cpp
    ${SIMULATOR_HAL}
    ${CROSSPOINT_SRC}
    ${CROSSPOINT_LIB}
    ${VENDOR_C}
    ${REPO_ROOT}/lib/I18n/I18nStrings.cpp
)
add_dependencies(crosspoint_simulator i18n_generated)

target_link_libraries(crosspoint_simulator PRIVATE arduino_host ArduinoJson)

if(NOT EMSCRIPTEN)
    target_link_libraries(crosspoint_simulator PRIVATE CURL::libcurl)
    if(SDL2_FOUND)
        target_link_libraries(crosspoint_simulator PRIVATE SDL2::SDL2)
    else()
        target_compile_options(crosspoint_simulator PRIVATE ${SDL2_CFLAGS})
        target_link_options(crosspoint_simulator PRIVATE ${SDL2_LDFLAGS})
    endif()
endif()

target_include_directories(crosspoint_simulator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
# Real JPEGDEC before shims/ so native builds don't pick up the stub header.
if(NOT EMSCRIPTEN)
    target_include_directories(crosspoint_simulator PRIVATE ${JPEGDEC_ROOT}/src)
endif()
target_include_directories(crosspoint_simulator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/shims
    ${REPO_ROOT}/src
    ${REPO_ROOT}/lib/hal
    ${REPO_ROOT}/lib/Logging
    ${REPO_ROOT}/lib/Memory
    ${REPO_ROOT}/lib/MiniBidi
    ${REPO_ROOT}/lib/I18n
    ${REPO_ROOT}/lib/GfxRenderer
    ${REPO_ROOT}/lib/EpdFont
    ${REPO_ROOT}/lib/EpdFont/builtinFonts
    ${REPO_ROOT}/lib/Epub
    ${REPO_ROOT}/lib/Epub/Epub
    ${REPO_ROOT}/lib/FsHelpers
    ${REPO_ROOT}/lib/InflateReader
    ${REPO_ROOT}/lib/JpegToBmpConverter
    ${REPO_ROOT}/lib/JsonParser
    ${REPO_ROOT}/lib/OpdsParser
    ${REPO_ROOT}/lib/PngToBmpConverter
    ${REPO_ROOT}/lib/Serialization
    ${REPO_ROOT}/lib/Txt
    ${REPO_ROOT}/lib/Utf8
    ${REPO_ROOT}/lib/XmlParserUtils
    ${REPO_ROOT}/lib/Xtc
    ${REPO_ROOT}/lib/ZipFile
    ${REPO_ROOT}/lib/expat
    ${REPO_ROOT}/lib/uzlib/src
    ${qrcode_SOURCE_DIR}/src
    ${REPO_ROOT}/lib/KOReaderSync
    ${REPO_ROOT}/lib/Epub/Epub/converters
)

# Rename Arduino entry points so the simulator entry can drive them explicitly.
# Default SKU is Traditional Chinese (CJK fonts + WeRead/农历/CJK typography).
# SIMULATOR_JAPANESE_VERSION / SIMULATOR_KOREAN_VERSION switch to those firmwares.
# SIMULATOR_CHINESE_UI_SIMPLIFIED mirrors PlatformIO's CHINESE_UI_SIMPLIFIED (SC SKU).
target_compile_definitions(crosspoint_simulator PRIVATE
    CROSSPOINT_EMULATED=1
    EINK_DISPLAY_SINGLE_BUFFER_MODE=1
    LOG_LEVEL=2
    ENABLE_SERIAL_LOG=1
    XML_GE=0
    XML_CONTEXT_BYTES=1024
    USE_UTF8_LONG_NAMES=1
    CROSSPOINT_VERSION="simulator-dev"
    "setup=firmware_setup"
    "loop=firmware_loop"
)
if(SIMULATOR_JAPANESE_VERSION)
    target_compile_definitions(crosspoint_simulator PRIVATE
        ENABLE_JAPANESE_VERSION=1
        ENABLE_CJK_VERSION=1)
elseif(SIMULATOR_KOREAN_VERSION)
    target_compile_definitions(crosspoint_simulator PRIVATE
        ENABLE_KOREAN_VERSION=1
        ENABLE_CJK_VERSION=1)
else()
    target_compile_definitions(crosspoint_simulator PRIVATE
        ENABLE_CHINESE_VERSION=1
        ENABLE_CJK_VERSION=1)
    if(SIMULATOR_CHINESE_UI_SIMPLIFIED)
        target_compile_definitions(crosspoint_simulator PRIVATE CHINESE_UI_SIMPLIFIED=1)
    endif()
endif()

target_compile_options(crosspoint_simulator PRIVATE -fno-exceptions -fno-rtti
    -Wno-unused-parameter -Wno-deprecated-declarations)
# -Wno-bidi-chars exists on GCC (PlatformIO) but not Apple Clang — only add if supported.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wno-bidi-chars HAS_WNO_BIDI_CHARS)
if(HAS_WNO_BIDI_CHARS)
    target_compile_options(crosspoint_simulator PRIVATE -Wno-bidi-chars)
endif()
# Host clang does not get Arduino/ESP transitive includes that pull in
# <cmath>/<algorithm>/<cstdint>. Force them so firmware sources that use
# std::round/std::clamp/uint32_t compile without ESP toolchain headers.
if(NOT EMSCRIPTEN)
    target_compile_options(crosspoint_simulator PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/force_includes.h>")
endif()

# -----------------------------------------------------------------------------
# WebAssembly (Emscripten) link configuration.
#   - pthreads (Web Workers + SharedArrayBuffer) back FreeRTOS-on-std::thread; this
#     requires COOP/COEP headers when served (see crosspoint-web).
#   - the demo SD card (one small public-domain epub + minimal .crosspoint cache) is
#     preloaded into MEMFS at /sd via --preload-file.
#   - EXIT_RUNTIME=0 keeps the runtime alive after main() returns so the firmware
#     worker keeps running and the exported functions stay callable.
#   - output is crosspoint_simulator_wasm.{js,wasm,data}, MODULARIZE'd as
#     createSimulator() for clean loading from the browser page.
# -----------------------------------------------------------------------------
if(EMSCRIPTEN)
    # -pthread is already applied at directory scope (top of this file) and inherited here for
    # both compile and link, so it is not repeated on the target.
    set(SIMULATOR_SD_DEMO ${CMAKE_CURRENT_SOURCE_DIR}/sd_root_demo)
    target_link_options(crosspoint_simulator PRIVATE
        -sPTHREAD_POOL_SIZE=6
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=64MB
        -sMAXIMUM_MEMORY=512MB
        -sEXIT_RUNTIME=0
        -sMODULARIZE=1
        -sEXPORT_NAME=createSimulator
        -sENVIRONMENT=web,worker
        "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,HEAPU8,FS"
        "-sEXPORTED_FUNCTIONS=_main,_malloc,_free,_simulator_framebuffer_ptr,_simulator_consume_dirty,_simulator_display_width,_simulator_display_height,_simulator_buffer_size,_simulator_inject_button"
        "--preload-file=${SIMULATOR_SD_DEMO}@/sd"
    )
    set_target_properties(crosspoint_simulator PROPERTIES
        OUTPUT_NAME "crosspoint_simulator_wasm"
        SUFFIX ".js")
endif()

