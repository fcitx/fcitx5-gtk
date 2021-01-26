set(FCITX_GTK4_IM_MODULE_SOURCES
  fcitxim.c
  fcitximcontext.cpp
  inputwindow.cpp
  gtk4inputwindow.cpp
  fcitxtheme.cpp
  utils.cpp
  )

pkg_check_modules(Gtk4 REQUIRED IMPORTED_TARGET "gtk4")
pkg_check_modules(Gtk4X11 REQUIRED IMPORTED_TARGET "gtk4-x11")
pkg_get_variable(GTK4_BINARY_VERSION "gtk4" "gtk_binary_version")

if (NOT DEFINED GTK4_IM_MODULEDIR)
    set(GTK4_IM_MODULEDIR "${CMAKE_INSTALL_LIBDIR}/gtk-4.0/${GTK4_BINARY_VERSION}/immodules" CACHE PATH "Gtk4 im module directory")
endif()

add_library(im-fcitx5-gtk4 MODULE ${FCITX_GTK4_IM_MODULE_SOURCES})
set_target_properties(im-fcitx5-gtk4 PROPERTIES OUTPUT_NAME "im-fcitx5"
  COMPILE_FLAGS "-fno-exceptions")
target_link_libraries(im-fcitx5-gtk4 Fcitx5::GClient XKBCommon::XKBCommon PkgConfig::Gtk4 PkgConfig::Gtk4X11 X11Import)
install(TARGETS im-fcitx5-gtk4 DESTINATION "${GTK4_IM_MODULEDIR}")
