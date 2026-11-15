# --- Detect if the plugin is build out of tree or not ---
if(CMAKE_PROJECT_NAME STREQUAL "obs-studio")
  set(BUILD_OUT_OF_TREE OFF)
else()
  set(BUILD_OUT_OF_TREE ON)
  cmake_minimum_required(VERSION 3.16...3.26)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

# buildspec.json holds the version; the plugin manifest OBS reads has to say the same.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/data/manifest.json" _manifest)
string(JSON _manifest_version GET "${_manifest}" version)
if(NOT _manifest_version STREQUAL "${_version}")
  message(FATAL_ERROR "data/manifest.json says version ${_manifest_version} but buildspec.json says ${_version}")
endif()

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${PROJECT_NAME} MODULE)

if(BUILD_OUT_OF_TREE)
  find_package(libobs REQUIRED)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE OBS::frontend-api)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE OBS::libobs)

find_package(Qt6 COMPONENTS Widgets Core)
if(BUILD_OUT_OF_TREE)
  if(OS_LINUX OR OS_FREEBSD OR OS_OPENBSD)
    find_package(Qt6 REQUIRED Gui)
  endif()
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Core Qt::Widgets)

# libcurl ships with OBS; the bot link uses its WebSocket client.
if(BUILD_OUT_OF_TREE)
  find_package(CURL REQUIRED)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)
if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()

if((OS_LINUX OR OS_FREEBSD OR OS_OPENBSD) AND Qt6_VERSION VERSION_LESS "6.9.0")
  find_package(Qt6 COMPONENTS GuiPrivate)
  target_link_libraries(${PROJECT_NAME} PRIVATE Qt::GuiPrivate)
endif()

target_compile_options(
${PROJECT_NAME} PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header
                                -Wno-comma>)
set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set_target_properties(
${PROJECT_NAME}
PROPERTIES AUTOMOC ON
            AUTOUIC ON
            AUTORCC ON)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h)

if(OS_WINDOWS)
  configure_file(cmake/windows/resources/installer-Windows.iss.in "${CMAKE_CURRENT_BINARY_DIR}/installer-Windows.generated.iss")
endif()

target_sources(${PROJECT_NAME} PRIVATE
  src/fiji-suite.cpp
  src/component-source.c
  resources.qrc
  src/version.h)

target_sources(${PROJECT_NAME} PRIVATE
  src/docks/canvas-dock.cpp
  src/docks/canvas-dock.hpp
  src/docks/canvas-clone-dock.cpp
  src/docks/canvas-clone-dock.hpp
  src/docks/capture-dock.cpp
  src/docks/capture-dock.hpp
  src/docks/filters-dock.cpp
  src/docks/filters-dock.hpp
  src/docks/live-scenes-dock.cpp
  src/docks/live-scenes-dock.hpp
  src/docks/multiview-dock.cpp
  src/docks/multiview-dock.hpp
  src/docks/output-dock.cpp
  src/docks/output-dock.hpp
  src/docks/properties-dock.cpp
  src/docks/properties-dock.hpp
  src/docks/scenes-dock.cpp
  src/docks/scenes-dock.hpp
  src/docks/sources-dock.cpp
  src/docks/sources-dock.hpp
  src/docks/stats-dock.cpp
  src/docks/stats-dock.hpp
  src/docks/transform-dock.cpp
  src/docks/transform-dock.hpp
  src/docks/transitions-dock.cpp
  src/docks/transitions-dock.hpp)

target_sources(${PROJECT_NAME} PRIVATE
  src/decks/deck-manager.cpp
  src/decks/deck-manager.hpp
  src/decks/engine-core.cpp
  src/decks/engine-core.hpp)

target_sources(${PROJECT_NAME} PRIVATE
  src/dialogs/config-dialog.cpp
  src/dialogs/config-dialog.hpp
  src/dialogs/ffmpeg-output-dialog.cpp
  src/dialogs/ffmpeg-output-dialog.hpp
  src/dialogs/name-dialog.cpp
  src/dialogs/name-dialog.hpp
  src/dialogs/record-output-dialog.cpp
  src/dialogs/record-output-dialog.hpp
  src/dialogs/stream-output-dialog.cpp
  src/dialogs/stream-output-dialog.hpp)

target_sources(${PROJECT_NAME} PRIVATE
  src/utils/bot-link.cpp
  src/utils/bot-link.hpp
  src/utils/color.cpp
  src/utils/event-filter.cpp
  src/utils/icon.cpp
  src/utils/obs-websocket.cpp
  src/utils/widgets/accessible-alignment-cell.cpp
  src/utils/widgets/accessible-alignment-selector.cpp
  src/utils/widgets/alignment-selector.cpp
  src/utils/widgets/capture-widget.cpp
  src/utils/widgets/double-slider.cpp
  src/utils/widgets/focus-scroll-spinbox.cpp
  src/utils/widgets/hotkey-edit.cpp
  src/utils/widgets/locked-checkbox.cpp
  src/utils/widgets/output-widget.cpp
  src/utils/widgets/pixmap-label.cpp
  src/utils/widgets/scene-list-widget-item.cpp
  src/utils/widgets/slider-ignore-scroll.cpp
  src/utils/widgets/source-tree.cpp
  src/utils/widgets/stream-key-input.cpp
  src/utils/widgets/switching-splitter.cpp
  src/utils/widgets/projector.cpp
  src/utils/widgets/qt-display.cpp
  src/utils/widgets/visibility-checkbox.cpp
  src/utils/widgets/visibility-item-widget.cpp
  src/utils/color.hpp
  src/utils/event-filter.hpp
  src/utils/icon.hpp
  src/utils/widgets/accessible-alignment-cell.hpp
  src/utils/widgets/accessible-alignment-selector.hpp
  src/utils/widgets/alignment-selector.hpp
  src/utils/widgets/capture-widget.hpp
  src/utils/widgets/double-slider.hpp
  src/utils/widgets/focus-scroll-spinbox.hpp
  src/utils/widgets/hotkey-edit.hpp
  src/utils/widgets/locked-checkbox.hpp
  src/utils/widgets/output-widget.hpp
  src/utils/widgets/pixmap-label.hpp
  src/utils/widgets/scene-list-widget-item.hpp
  src/utils/widgets/slider-ignore-scroll.hpp
  src/utils/widgets/source-tree.hpp
  src/utils/widgets/stream-key-input.hpp
  src/utils/widgets/switching-splitter.hpp
  src/utils/widgets/projector.hpp
  src/utils/widgets/qt-display.hpp
  src/utils/widgets/visibility-checkbox.hpp
  src/utils/widgets/visibility-item-widget.hpp)

if(BUILD_OUT_OF_TREE)
	set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
else()
	set_target_properties_obs(${PROJECT_NAME} PROPERTIES FOLDER "plugins/fiji" PREFIX "")
endif()

# Unit tests for the deck engine core, which depends on Qt Core only.
#   cmake --build build_x64 --config RelWithDebInfo --target decks-engine-test
if(BUILD_OUT_OF_TREE)
  add_executable(decks-engine-test tests/engine-test.cpp src/decks/engine-core.cpp)
  target_link_libraries(decks-engine-test PRIVATE Qt::Core)
  set_target_properties(decks-engine-test PROPERTIES EXCLUDE_FROM_ALL ON)
endif()
