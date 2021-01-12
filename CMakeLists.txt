cmake_minimum_required(VERSION 3.6)
project(fcitx5-gtk VERSION 5.0.3)

find_package(ECM REQUIRED 1.0.0)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

include(FindPkgConfig)
include(ECMSetupVersion)
include(GenerateExportHeader)
include(FeatureSummary)
include(GNUInstallDirs)
include(ECMUninstallTarget)

option(ENABLE_GIR "GObject Introspection" ON)
option(ENABLE_GTK2_IM_MODULE "Enable GTK2 IM Module" ON)
option(ENABLE_GTK3_IM_MODULE "Enable GTK3 IM Module" ON)
option(ENABLE_GTK4_IM_MODULE "Enable GTK4 IM Module" ON)
option(ENABLE_SNOOPER "Enable Key Snooper for gtk app" ON)

set(NO_SNOOPER_APPS ".*chrome.*,.*chromium.*,firefox.*,Do.*"
    CACHE STRING "Disable Key Snooper for following app by default.")
set(NO_PREEDIT_APPS "gvim.*" CACHE STRING "Disable preedit for follwing app by default.")
set(SYNC_MODE_APPS "firefox.*" CACHE STRING "Use sync mode for following app by default.")

configure_file(config.h.in "${CMAKE_CURRENT_BINARY_DIR}/config.h")
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
find_package(PkgConfig)
find_package(XKBCommon)
pkg_check_modules(GLib2 REQUIRED IMPORTED_TARGET "glib-2.0>=2.38")
pkg_check_modules(Gio2 REQUIRED IMPORTED_TARGET "gio-2.0")
pkg_check_modules(GObject2 REQUIRED IMPORTED_TARGET "gobject-2.0")
add_subdirectory(fcitx-gclient)

set(CMAKE_C_VISIBILITY_PRESET default)

include(cmake/CompilerSettings.cmake)

if (ENABLE_GTK2_IM_MODULE OR ENABLE_GTK3_IM_MODULE OR ENABLE_GTK4_IM_MODULE)
find_package(X11 REQUIRED)
add_library(X11Import UNKNOWN IMPORTED)
set_target_properties(X11Import PROPERTIES
    IMPORTED_LOCATION "${X11_X11_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${X11_X11_INCLUDE_PATH}")
endif()

if (ENABLE_GTK2_IM_MODULE)
add_subdirectory(gtk2)
endif()

if (ENABLE_GTK3_IM_MODULE)
add_subdirectory(gtk3)
endif()

if (ENABLE_GTK4_IM_MODULE)
add_subdirectory(gtk4)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
