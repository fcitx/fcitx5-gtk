set(FCITX_GTK3_IM_MODULE_SOURCES
  fcitxim.c
  fcitximcontext.cpp
  fcitxtheme.cpp
  utils.cpp
  inputwindow.cpp
  gtk3inputwindow.cpp
  )

pkg_check_modules(Gtk3 REQUIRED IMPORTED_TARGET "gtk+-3.0")
pkg_check_modules(Gdk3 REQUIRED IMPORTED_TARGET "gdk-3.0")
pkg_check_modules(Gdk3X11 REQUIRED IMPORTED_TARGET "gdk-x11-3.0")
pkg_get_variable(GTK3_BINARY_VERSION "gtk+-3.0" "gtk_binary_version")

if (NOT DEFINED GTK3_IM_MODULEDIR)
    set(GTK3_IM_MODULEDIR "${CMAKE_INSTALL_LIBDIR}/gtk-3.0/${GTK3_BINARY_VERSION}/immodules" CACHE PATH "Gtk3 im module directory")
endif()

add_library(im-fcitx5-gtk3 MODULE ${FCITX_GTK3_IM_MODULE_SOURCES})
set_target_properties(im-fcitx5-gtk3 PROPERTIES PREFIX "" OUTPUT_NAME "im-fcitx5"
  COMPILE_FLAGS "-fno-exceptions")
target_link_libraries(im-fcitx5-gtk3 Fcitx5::GClient XKBCommon::XKBCommon PkgConfig::Gtk3 PkgConfig::Gdk3 PkgConfig::Gdk3X11 X11Import)
install(TARGETS im-fcitx5-gtk3 DESTINATION "${GTK3_IM_MODULEDIR}")
