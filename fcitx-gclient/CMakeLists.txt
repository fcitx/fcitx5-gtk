set(FCITX_GCLIENT_SOURCES
  fcitxgwatcher.c
  fcitxgclient.c
  )

set(FCITX_GCLIENT_BUILT_SOURCES
  ${CMAKE_CURRENT_BINARY_DIR}/marshall.c
  )

set(FCITX_GCLIENT_HEADERS
  fcitxgclient.h
  fcitxgwatcher.h
  )

set(FCITX_GCLIENT_BUILT_HEADERS
  ${CMAKE_CURRENT_BINARY_DIR}/marshall.h
  )

ecm_setup_version(PROJECT
                  VARIABLE_PREFIX Fcitx5GClient
                  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/Fcitx5GClientConfigVersion.cmake"
                  SOVERSION 1)

pkg_get_variable(GLIB2_GLIB_GENMARSHAL "glib-2.0" "glib_genmarshal")
find_program(GLIB_GENMARSHAL ${GLIB2_GLIB_GENMARSHAL})

add_custom_command(OUTPUT marshall.c
  COMMAND ${GLIB_GENMARSHAL} --body --prefix=fcitx_marshall --internal --include-header=marshall.h
  ${PROJECT_SOURCE_DIR}/gtk-common/marshall.list > marshall.c
  DEPENDS ${PROJECT_SOURCE_DIR}/gtk-common/marshall.list)
add_custom_command(OUTPUT marshall.h
  COMMAND ${GLIB_GENMARSHAL} --header --prefix=fcitx_marshall --internal
  ${PROJECT_SOURCE_DIR}/gtk-common/marshall.list > marshall.h
  DEPENDS ${PROJECT_SOURCE_DIR}/gtk-common/marshall.list)

add_library(Fcitx5GClient SHARED ${FCITX_GCLIENT_SOURCES}
  ${FCITX_GCLIENT_BUILT_SOURCES} ${FCITX_GCLIENT_BUILT_HEADERS})
set_target_properties(Fcitx5GClient
  PROPERTIES VERSION ${Fcitx5GClient_VERSION}
  SOVERSION ${Fcitx5GClient_SOVERSION}
  EXPORT_NAME GClient
  C_VISIBILITY_PRESET default
  )
target_include_directories(Fcitx5GClient
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..;${CMAKE_CURRENT_BINARY_DIR}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_FULL_INCLUDEDIR}>/Fcitx5/GClient"
    INTERFACE
        "${Gio2_INCLUDE_DIRS}"
        "${GObject2_INCLUDE_DIRS}"
)
target_link_libraries(Fcitx5GClient LINK_PRIVATE PkgConfig::Gio2 PkgConfig::GLib2 PkgConfig::GObject2)

configure_file(Fcitx5GClient.pc.in ${CMAKE_CURRENT_BINARY_DIR}/Fcitx5GClient.pc @ONLY)

install(TARGETS Fcitx5GClient EXPORT Fcitx5GClientTargets LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Fcitx5GClient.pc
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
install(FILES ${FCITX_GCLIENT_HEADERS} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/Fcitx5/GClient/fcitx-gclient")

add_library(Fcitx5::GClient ALIAS Fcitx5GClient)

configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/Fcitx5GClientConfig.cmake.in"
                              "${CMAKE_CURRENT_BINARY_DIR}/Fcitx5GClientConfig.cmake"
                              INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Fcitx5GClient
)

install(EXPORT Fcitx5GClientTargets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Fcitx5GClient FILE Fcitx5GClientTargets.cmake NAMESPACE Fcitx5::)

install(FILES  "${CMAKE_CURRENT_BINARY_DIR}/Fcitx5GClientConfig.cmake"
               "${CMAKE_CURRENT_BINARY_DIR}/Fcitx5GClientConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Fcitx5GClient
        COMPONENT Devel )

if(ENABLE_GIR)
  include(GObjectIntrospection)
  gobject_introspection(
    FILENAME FcitxG-1.0.gir
    NSVERSION 1.0
    INCLUDE Gio-2.0 GObject-2.0 GLib-2.0
    PACKAGE_EXPORT Fcitx5GClient
    LIBRARY Fcitx5GClient
    NAMESPACE FcitxG
    SCANNER_ARGS --warn-all --add-include-path=${CMAKE_CURRENT_SOURCE_DIR}
    COMPILER_ARGS "--includedir=${CMAKE_CURRENT_SOURCE_DIR}"
    SYMBOL_PREFIXES fcitx_g
    SOURCES ${FCITX_GCLIENT_SOURCES} ${FCITX_GCLIENT_HEADERS}
    QUIET
    )
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/FcitxG-1.0.gir"
    DESTINATION "${GOBJECT_INTROSPECTION_GIRDIR}")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/FcitxG-1.0.typelib"
    DESTINATION "${GOBJECT_INTROSPECTION_TYPELIBDIR}")
endif()
