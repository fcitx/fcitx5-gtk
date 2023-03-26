set(FCITX_GTK2_IM_MODULE_SOURCES
  fcitxim.c
  fcitximcontext.cpp
  )

if (NOT DEFINED GTK2_IM_MODULEDIR)
    set(GTK2_IM_MODULEDIR "${CMAKE_INSTALL_LIBDIR}/gtk-2.0/${GTK2_BINARY_VERSION}/immodules" CACHE PATH "Gtk2 im module directory")
endif()

add_library(im-fcitx5 MODULE ${FCITX_GTK2_IM_MODULE_SOURCES})
set_target_properties( im-fcitx5 PROPERTIES PREFIX ""
  COMPILE_FLAGS "-fno-exceptions")
target_link_libraries(im-fcitx5 Fcitx5::GClient XKBCommon::XKBCommon PkgConfig::Gtk2 PkgConfig::Gdk2 PkgConfig::Gdk2X11 X11Import)
install(TARGETS im-fcitx5 DESTINATION "${GTK2_IM_MODULEDIR}")


if (NOT BUILD_ONLY_PLUGIN)
    add_executable(fcitx5-gtk2-immodule-probing immodule-probing.cpp)
    target_link_libraries(fcitx5-gtk2-immodule-probing PkgConfig::Gtk2)
    install(TARGETS fcitx5-gtk2-immodule-probing DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
