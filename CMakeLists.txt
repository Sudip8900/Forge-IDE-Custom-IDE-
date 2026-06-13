cmake_minimum_required(VERSION 3.15)
project(ForgeIDE LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Speed up compilations by using multithreaded builds in MSVC
if(MSVC)
    add_compile_options(/MP)
endif()

include(FetchContent)

# -----------------------------------------------------------------------------
# 1. Fetch GLFW
# -----------------------------------------------------------------------------
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
FetchContent_MakeAvailable(glfw)

# -----------------------------------------------------------------------------
# 2. Fetch Dear ImGui (Docking Branch)
# -----------------------------------------------------------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
    
    # Define ImGui static library compilation target
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    
    target_include_directories(imgui PUBLIC 
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    
    # Link ImGui to GLFW and OpenGL
    target_link_libraries(imgui PUBLIC glfw)
    if(NOT APPLE)
        target_link_libraries(imgui PUBLIC opengl32)
    endif()
endif()

# -----------------------------------------------------------------------------
# 3. ForgeIDE Binary Target
# -----------------------------------------------------------------------------
set(SOURCE_FILES
    src/main.cpp
    src/core/Engine.hpp
    src/core/Engine.cpp
    src/core/Workspace.hpp
    src/core/Workspace.cpp
    src/utils/Logger.hpp
    src/utils/Logger.cpp
    src/ui/Panel.hpp
    src/ui/UIManager.hpp
    src/ui/UIManager.cpp
    src/ui/panels/EditorPanel.hpp
    src/ui/panels/EditorPanel.cpp
    src/ui/panels/FileExplorerPanel.hpp
    src/ui/panels/FileExplorerPanel.cpp
    src/ui/panels/OtherPanels.hpp
    
    # Terminal Subsystem
    src/ui/terminal/TerminalCell.hpp
    src/ui/terminal/ConPTY.hpp
    src/ui/terminal/ConPTY.cpp
    src/ui/terminal/TerminalEmulator.hpp
    src/ui/terminal/TerminalEmulator.cpp
    src/ui/terminal/TerminalSession.hpp
    src/ui/terminal/TerminalSession.cpp
    src/ui/terminal/TerminalProfile.hpp
    src/ui/terminal/UnrealGitHelper.hpp
    src/ui/panels/TerminalPanel.hpp
    src/ui/panels/TerminalPanel.cpp
)

if(WIN32)
    list(APPEND SOURCE_FILES resources/resources.rc)
endif()

add_executable(ForgeIDE ${SOURCE_FILES})

# Include directories
target_include_directories(ForgeIDE PRIVATE
    src
)

# Link dependency libraries
target_link_libraries(ForgeIDE PRIVATE
    imgui
    glfw
)

# Platform specific linkages
if(WIN32)
    # Dwmapi for native dark mode window frame, Comdlg32 for native file dialogs
    target_link_libraries(ForgeIDE PRIVATE dwmapi comdlg32)
    
    # Prevent min/max macro definitions in windows.h from breaking std::min/std::max
    target_compile_definitions(ForgeIDE PRIVATE NOMINMAX)
    
    # Build as a standard Windows console/desktop app
    # Set entry point
    set_target_properties(ForgeIDE PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
