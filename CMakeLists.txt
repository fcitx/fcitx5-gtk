cmake_minimum_required(VERSION 3.6)
project(fcitx5-gtk VERSION 5.1.0)

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
option(BUILD_ONLY_PLUGIN "Build only IM Module" OFF)

set(NO_SNOOPER_APPS ".*chrome.*,.*chromium.*,firefox.*,Do.*"
    CACHE STRING "Disable Key Snooper for following app by default.")
set(NO_PREEDIT_APPS "gvim.*" CACHE STRING "Disable preedit for follwing app by default.")
set(SYNC_MODE_APPS "firefox.*" CACHE STRING "Use sync mode for following app by default.")

configure_file(config.h.in "${CMAKE_CURRENT_BINARY_DIR}/config.h")
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
find_package(PkgConfig)
find_package(XKBCommon)
pkg_check_modules(GLib2 REQUIRED IMPORTED_TARGET "glib-2.0>=2.56")
pkg_check_modules(Gio2 REQUIRED IMPORTED_TARGET "gio-2.0")
pkg_check_modules(GioUnix2 REQUIRED IMPORTED_TARGET "gio-unix-2.0")
pkg_check_modules(GObject2 REQUIRED IMPORTED_TARGET "gobject-2.0")

set(CMAKE_C_VISIBILITY_PRESET default)

include(cmake/CompilerSettings.cmake)

if (BUILD_ONLY_PLUGIN)
    set(GCLIENT_LIBRARY_TYPE STATIC)
else()
    set(GCLIENT_LIBRARY_TYPE SHARED)
endif()

add_subdirectory(fcitx-gclient)

set(NEED_X11 FALSE)
if (ENABLE_GTK2_IM_MODULE)
    pkg_check_modules(Gtk2 REQUIRED IMPORTED_TARGET "gtk+-2.0")
    pkg_check_modules(Gdk2 REQUIRED IMPORTED_TARGET "gdk-2.0")
    pkg_check_modules(Gdk2X11 REQUIRED IMPORTED_TARGET "gdk-x11-2.0")
    pkg_get_variable(GTK2_BINARY_VERSION "gtk+-2.0" "gtk_binary_version")
    set(NEED_X11 TRUE)
endif()

if (ENABLE_GTK3_IM_MODULE)
    pkg_check_modules(Gtk3 REQUIRED IMPORTED_TARGET "gtk+-3.0")
    pkg_check_modules(Gdk3 REQUIRED IMPORTED_TARGET "gdk-3.0")
    pkg_get_variable(GTK3_BINARY_VERSION "gtk+-3.0" "gtk_binary_version")
    pkg_get_variable(GTK3_TARGETS "gtk+-3.0" "targets")
    if (GTK3_TARGETS MATCHES "x11")
        set(NEED_X11 TRUE)
        pkg_check_modules(Gdk3X11 REQUIRED IMPORTED_TARGET "gdk-x11-3.0")
    endif()
endif()

if (ENABLE_GTK4_IM_MODULE)
    pkg_check_modules(Gtk4 REQUIRED IMPORTED_TARGET "gtk4>=4.2")
    pkg_get_variable(GTK4_BINARY_VERSION "gtk4" "gtk_binary_version")
    pkg_get_variable(GTK4_TARGETS "gtk4" "targets")
    if (GTK4_TARGETS MATCHES "x11")
        set(NEED_X11 TRUE)
        pkg_check_modules(Gtk4X11 REQUIRED IMPORTED_TARGET "gtk4-x11")
    endif()
endif()

if (NEED_X11)
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
