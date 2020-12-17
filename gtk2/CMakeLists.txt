set(FCITX_GTK2_IM_MODULE_SOURCES
  fcitxim.c
  fcitximcontext.cpp
  )

pkg_check_modules(Gtk2 REQUIRED IMPORTED_TARGET "gtk+-2.0")
pkg_check_modules(Gdk2 REQUIRED IMPORTED_TARGET "gdk-2.0")
pkg_check_modules(Gdk2X11 REQUIRED IMPORTED_TARGET "gdk-x11-2.0")
pkg_get_variable(GTK2_BINARY_VERSION "gtk+-2.0" "gtk_binary_version")

if (NOT DEFINED GTK2_IM_MODULEDIR)
    set(GTK2_IM_MODULEDIR "${CMAKE_INSTALL_LIBDIR}/gtk-2.0/${GTK2_BINARY_VERSION}/immodules" CACHE PATH "Gtk2 im module directory")
endif()

add_library(im-fcitx5 MODULE ${FCITX_GTK2_IM_MODULE_SOURCES})
set_target_properties( im-fcitx5 PROPERTIES PREFIX ""
  COMPILE_FLAGS "-fno-exceptions")
target_link_libraries(im-fcitx5 Fcitx5::GClient XKBCommon::XKBCommon PkgConfig::Gtk2 PkgConfig::Gdk2 PkgConfig::Gdk2X11 X11Import)
install(TARGETS im-fcitx5 DESTINATION "${GTK2_IM_MODULEDIR}")
