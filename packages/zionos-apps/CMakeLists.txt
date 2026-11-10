cmake_minimum_required(VERSION 3.25)
project(zionos-apps VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(GNUInstallDirs)

option(ZIONOS_SANITIZE "Build with AddressSanitizer and UBSan" OFF)
option(ZIONOS_WERROR "Treat compiler warnings as errors" OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Network DBus LinguistTools Test)
find_package(Qt6Keychain REQUIRED)

add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wnon-virtual-dtor
                    -Wold-style-cast -Wformat=2)
add_compile_definitions(QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII QT_NO_URL_CAST_FROM_STRING
                        QT_DISABLE_DEPRECATED_BEFORE=0x060400
                        ZIONOS_VERSION="${PROJECT_VERSION}")
if(ZIONOS_WERROR)
  add_compile_options(-Werror)
endif()
if(ZIONOS_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

set(ZIONOS_DATADIR "${CMAKE_INSTALL_FULL_DATADIR}/zionos")
add_compile_definitions(ZIONOS_TRANSLATIONS_DIR="${ZIONOS_DATADIR}/translations")

option(ZIONOS_BUILD_GUI "Build the graphical applications" ON)
add_subdirectory(core)
if(ZIONOS_BUILD_GUI)
  add_subdirectory(ui)
  add_subdirectory(welcome)
  add_subdirectory(settings)
  add_subdirectory(assistant)
endif()

# Translations: English is the source language; Hebrew is maintained in
# translations/zionos_he.ts. `cmake --build . --target update_translations`
# refreshes the .ts files from the sources.
file(GLOB_RECURSE ZION_TR_SOURCES CONFIGURE_DEPENDS
     core/*.cpp core/*.h ui/*.cpp ui/*.h welcome/*.cpp settings/*.cpp assistant/*.cpp assistant/*.h)
qt_add_lupdate(TS_FILES translations/zionos_he.ts SOURCES ${ZION_TR_SOURCES}
               OPTIONS -no-obsolete -locations none)
qt_add_lrelease(TS_FILES translations/zionos_he.ts QM_FILES_OUTPUT_VARIABLE ZION_QM)
install(FILES ${ZION_QM} DESTINATION ${ZIONOS_DATADIR}/translations)

if(ZIONOS_BUILD_GUI)
install(FILES data/zionos-welcome.desktop data/zionos-settings.desktop
              data/zionos-assistant.desktop data/zionos-about.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/zionos-welcome-autostart.desktop
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/xdg/autostart
        RENAME zionos-welcome.desktop)
endif()

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
