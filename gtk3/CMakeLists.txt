set(FCITX_GTK3_IM_MODULE_SOURCES
  fcitxim.c
  fcitximcontext.cpp
  fcitxtheme.cpp
  utils.cpp
  inputwindow.cpp
  gtk3inputwindow.cpp
  )

if (NOT DEFINED GTK3_IM_MODULEDIR)
    set(GTK3_IM_MODULEDIR "${CMAKE_INSTALL_LIBDIR}/gtk-3.0/${GTK3_BINARY_VERSION}/immodules" CACHE PATH "Gtk3 im module directory")
endif()

add_library(im-fcitx5-gtk3 MODULE ${FCITX_GTK3_IM_MODULE_SOURCES})
set_target_properties(im-fcitx5-gtk3 PROPERTIES PREFIX "" OUTPUT_NAME "im-fcitx5"
  COMPILE_FLAGS "-fno-exceptions")

target_link_libraries(im-fcitx5-gtk3 Fcitx5::GClient XKBCommon::XKBCommon PkgConfig::Gtk3 PkgConfig::GioUnix2)
if (TARGET PkgConfig::Gdk3X11)
target_link_libraries(im-fcitx5-gtk3 PkgConfig::Gdk3X11 X11Import)
endif()

install(TARGETS im-fcitx5-gtk3 DESTINATION "${GTK3_IM_MODULEDIR}")
