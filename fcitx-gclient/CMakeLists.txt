set(FCITX_GCLIENT_SOURCES
  fcitxclient.c
  fcitxconnection.c
  #fcitxdbusproxy.c
  )

set(FCITX_GCLIENT_BUILT_SOURCES
  ${CMAKE_CURRENT_BINARY_DIR}/marshall.c
  )

set(FCITX_GCLIENT_HEADERS
  fcitxclient.h
  fcitxconnection.h
  )

set(FCITX_GCLIENT_BUILT_HEADERS
  ${CMAKE_CURRENT_BINARY_DIR}/marshall.h
  )

pkg_get_variable(GLIB2_GLIB_GENMARSHAL "glib-2.0" "glib_genmarshal")
find_program(GLIB_GENMARSHAL ${GLIB2_GLIB_GENMARSHAL})

add_custom_command(OUTPUT marshall.c
  COMMAND ${GLIB_GENMARSHAL} --body --prefix=fcitx_marshall
  ${PROJECT_SOURCE_DIR}/gtk-common/marshall.list > marshall.c
  DEPENDS ${PROJECT_SOURCE_DIR}/gtk-common/marshall.list)
add_custom_command(OUTPUT marshall.h
  COMMAND ${GLIB_GENMARSHAL} --header --prefix=fcitx_marshall
  ${PROJECT_SOURCE_DIR}/gtk-common/marshall.list > marshall.h
  DEPENDS ${PROJECT_SOURCE_DIR}/gtk-common/marshall.list)

add_library(Fcitx5GClient SHARED ${FCITX_GCLIENT_SOURCES}
  ${FCITX_GCLIENT_BUILT_SOURCES} ${FCITX_GCLIENT_BUILT_HEADERS})
set_target_properties(Fcitx5GClient
  PROPERTIES VERSION 1.0
  SOVERSION 1
  COMPILE_FLAGS "-fvisibility=hidden"
  LINK_FLAGS "-Wl,--no-undefined"
  EXPORT_NAME GClient
  )
target_include_directories(Fcitx5GClient PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..;${CMAKE_CURRENT_BINARY_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_FULL_INCLUDEDIR}>/Fcitx5/GClient")
target_link_libraries(Fcitx5GClient PRIVATE PkgConfig::Gio2 PkgConfig::GLib2 PkgConfig::GObject2)

configure_file(Fcitx5GClient.pc.in ${CMAKE_CURRENT_BINARY_DIR}/Fcitx5GClient.pc @ONLY)

install(TARGETS Fcitx5GClient EXPORT Fcitx5GClientTargets LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Fcitx5GClient.pc
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
install(FILES ${FCITX_UTILS_HEADERS} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/Fcitx5/GClient/fcitx-gclient")

add_library(Fcitx5::GClient ALIAS Fcitx5GClient)

ecm_setup_version(PROJECT
                  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/Fcitx5GClientConfigVersion.cmake"
                  SOVERSION 1)

configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/Fcitx5GClientConfig.cmake.in"
                              "${CMAKE_CURRENT_BINARY_DIR}/Fcitx5GClientConfig.cmake"
                              INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Fcitx5GClient
)

    generate_export_header(Fcitx5GClient BASE_NAME FcitxGClient)

install(EXPORT Fcitx5GClientTargets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Fcitx5GClient FILE Fcitx5GClientTargets.cmake NAMESPACE Fcitx5::)

install(FILES  "${CMAKE_CURRENT_BINARY_DIR}/Fcitx5GClientConfig.cmake"
               "${CMAKE_CURRENT_BINARY_DIR}/Fcitx5GClientConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Fcitx5GClient
        COMPONENT Devel )

if(ENABLE_GIR)
  include(GObjectIntrospection)
  gobject_introspection(
    FILENAME Fcitx-1.0.gir
    NSVERSION 1.0
    INCLUDE Gio-2.0 GObject-2.0 GLib-2.0
    PACKAGE_EXPORT fcitx-gclient
    LIBRARY Fcitx5GClient
    SCANNER_ARGS --warn-all --add-include-path=${CMAKE_CURRENT_SOURCE_DIR}
    COMPILER_ARGS "--includedir=${CMAKE_CURRENT_SOURCE_DIR}"
    SYMBOL_PREFIXES fcitx
    SOURCES ${FCITX_GCLIENT_SOURCES} ${FCITX_GCLIENT_HEADERS}
    QUIET
    )
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/Fcitx-1.0.gir"
    DESTINATION "${GOBJECT_INTROSPECTION_GIRDIR}")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/Fcitx-1.0.typelib"
    DESTINATION "${GOBJECT_INTROSPECTION_TYPELIBDIR}")
endif()
