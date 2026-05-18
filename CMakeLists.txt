cmake_minimum_required(VERSION 3.24)

project(ArcanusOverlay VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ARCANUS_WITH_IMGUI "Build Dear ImGui rendering backend from third_party/imgui." ON)
option(ARCANUS_WITH_DX11_HOOK "Build DX11 Present hook using third_party/minhook." ON)
option(ARCANUS_WITH_GAME_DB "Build Core.zip game database indexer from third_party/miniz/json." ON)
option(ARCANUS_WITH_OLLAMA "Build local Ollama worker from third_party/cpp-httplib/json." ON)
option(ARCANUS_INPROCESS_OLLAMA "Allow the injected DLL to call Ollama directly. Unsafe; prefer external bridge." OFF)

add_library(arcanus_overlay SHARED
    src/dllmain.cpp
    src/arcanus/ArcanusApp.cpp
    src/arcanus/ai/TacticalAdvisor.cpp
    src/arcanus/core/DisplayNames.cpp
    src/arcanus/core/GameState.cpp
    src/arcanus/core/JsonWriter.cpp
    src/arcanus/core/RuntimePaths.cpp
    src/arcanus/core/Utf.cpp
    src/arcanus/db/GameDatabase.cpp
    src/arcanus/diagnostics/Diagnostics.cpp
    src/arcanus/llm/OllamaClient.cpp
    src/arcanus/memory/DataPointerProbe.cpp
    src/arcanus/memory/ReadOnlyScanner.cpp
    src/arcanus/memory/SafeMemory.cpp
    src/arcanus/overlay/HookController.cpp
    src/arcanus/overlay/OverlayRenderer.cpp
)

target_include_directories(arcanus_overlay PRIVATE src)

target_compile_definitions(arcanus_overlay PRIVATE
    ARCANUS_READ_ONLY=1
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(arcanus_overlay PRIVATE /W4 /EHsc /utf-8)
endif()

if(WIN32)
    target_link_libraries(arcanus_overlay PRIVATE user32 psapi dxgi d3d11 shcore ws2_32)
endif()

if(ARCANUS_WITH_IMGUI AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.h")
    target_compile_definitions(arcanus_overlay PRIVATE ARCANUS_WITH_IMGUI=1)
    target_include_directories(arcanus_overlay PRIVATE
        third_party/imgui
        third_party/imgui/backends
    )
    target_sources(arcanus_overlay PRIVATE
        third_party/imgui/imgui.cpp
        third_party/imgui/imgui_demo.cpp
        third_party/imgui/imgui_draw.cpp
        third_party/imgui/imgui_tables.cpp
        third_party/imgui/imgui_widgets.cpp
        third_party/imgui/backends/imgui_impl_dx11.cpp
        third_party/imgui/backends/imgui_impl_win32.cpp
    )
else()
    set(ARCANUS_WITH_IMGUI OFF)
endif()

if(ARCANUS_WITH_DX11_HOOK AND ARCANUS_WITH_IMGUI AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/minhook/include/MinHook.h")
    target_compile_definitions(arcanus_overlay PRIVATE ARCANUS_WITH_DX11_HOOK=1)
    target_include_directories(arcanus_overlay PRIVATE
        third_party/minhook/include
        third_party/minhook/src
    )
    target_sources(arcanus_overlay PRIVATE
        third_party/minhook/src/buffer.c
        third_party/minhook/src/hook.c
        third_party/minhook/src/trampoline.c
        third_party/minhook/src/hde/hde32.c
        third_party/minhook/src/hde/hde64.c
    )
else()
    set(ARCANUS_WITH_DX11_HOOK OFF)
endif()

if(ARCANUS_WITH_GAME_DB AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz/miniz.h" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/single_include/nlohmann/json.hpp")
    target_compile_definitions(arcanus_overlay PRIVATE ARCANUS_WITH_GAME_DB=1)
    target_include_directories(arcanus_overlay PRIVATE
        third_party/miniz
        third_party/json/single_include
    )
    target_sources(arcanus_overlay PRIVATE
        third_party/miniz/miniz.c
        third_party/miniz/miniz_zip.c
        third_party/miniz/miniz_tdef.c
        third_party/miniz/miniz_tinfl.c
    )
else()
    set(ARCANUS_WITH_GAME_DB OFF)
endif()

if(ARCANUS_WITH_OLLAMA AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cpp-httplib/httplib.h" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/single_include/nlohmann/json.hpp")
    target_compile_definitions(arcanus_overlay PRIVATE ARCANUS_WITH_OLLAMA=1)
    if(ARCANUS_INPROCESS_OLLAMA)
        target_compile_definitions(arcanus_overlay PRIVATE ARCANUS_INPROCESS_OLLAMA=1)
    endif()
    target_include_directories(arcanus_overlay PRIVATE
        third_party/cpp-httplib
        third_party/json/single_include
    )
else()
    set(ARCANUS_WITH_OLLAMA OFF)
endif()

add_executable(arcanus_loader WIN32
    src/loader/main.cpp
)

target_compile_features(arcanus_loader PRIVATE cxx_std_20)
target_compile_definitions(arcanus_loader PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(arcanus_loader PRIVATE /W4 /EHsc /utf-8)
endif()

if(WIN32)
    target_link_libraries(arcanus_loader PRIVATE user32 gdi32 shell32 advapi32)
endif()

add_custom_command(TARGET arcanus_loader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/PATCH_NOTES.md"
        "$<TARGET_FILE_DIR:arcanus_loader>/PATCH_NOTES.md"
)
