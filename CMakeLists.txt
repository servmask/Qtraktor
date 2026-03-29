cmake_minimum_required(VERSION 3.16)
project(Qtraktor VERSION 1.7.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# macOS deployment target (must be before find_package for proper SDK targeting)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# Vendored bzip2
add_library(bzip2 STATIC
    vendor/bzip2-1.0.8/blocksort.c
    vendor/bzip2-1.0.8/huffman.c
    vendor/bzip2-1.0.8/crctable.c
    vendor/bzip2-1.0.8/randtable.c
    vendor/bzip2-1.0.8/compress.c
    vendor/bzip2-1.0.8/decompress.c
    vendor/bzip2-1.0.8/bzlib.c
)
target_include_directories(bzip2 PUBLIC vendor/bzip2-1.0.8)
if(NOT MSVC)
    target_compile_options(bzip2 PRIVATE -w)
else()
    target_compile_options(bzip2 PRIVATE /w)
endif()

# Application sources
set(APP_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/backupfile.cpp
    src/cryptoutils.cpp
    src/clihandler.cpp
    src/mcpserver.cpp
    src/installcli.cpp
    src/agentconfig.cpp
    src/setupdialog.cpp
    src/passworddialog.cpp
    src/appdelegate.cpp
    src/dropoverlay.cpp
    src/extractionworker.cpp
    src/autoextractor.cpp
    src/progresswindow.cpp
)

set(APP_HEADERS
    src/mainwindow.h
    src/backupfile.h
    src/cryptoutils.h
    src/clihandler.h
    src/mcpserver.h
    src/installcli.h
    src/agentconfig.h
    src/setupdialog.h
    src/passworddialog.h
    src/appdelegate.h
    src/dropoverlay.h
    src/extractionworker.h
    src/autoextractor.h
    src/progresswindow.h
    src/dockprogress.h
)

set(APP_FORMS src/mainwindow.ui)

# Platform-specific sources
if(APPLE)
    enable_language(OBJCXX)
    list(APPEND APP_SOURCES src/dockprogress.mm)
else()
    list(APPEND APP_SOURCES src/dockprogress_stub.cpp)
endif()

qt_add_executable(Traktor ${APP_SOURCES} ${APP_HEADERS} ${APP_FORMS})

target_include_directories(Traktor PRIVATE src)

target_link_libraries(Traktor PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    bzip2
)

# macOS bundle configuration
if(APPLE)
    # Variables substituted into Info.plist.in via configure_file(@ONLY)
    set(EXECUTABLE_NAME "Traktor")
    set(PRODUCT_BUNDLE_IDENTIFIER "com.servmask.Traktor")
    set(QMAKE_PKGINFO_TYPEINFO "????")
    configure_file(Info.plist.in ${CMAKE_CURRENT_BINARY_DIR}/Info.plist @ONLY)

    set_target_properties(Traktor PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
    )

    target_link_libraries(Traktor PRIVATE
        "-framework AppKit"
        "-framework CoreServices"
        "-framework UniformTypeIdentifiers"
    )

    # Bundle icon files into Resources
    set(ICON_FILES icons/traktor.icns icons/file.icns)
    target_sources(Traktor PRIVATE ${ICON_FILES})
    set_source_files_properties(${ICON_FILES} PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
    )
endif()

# Windows configuration
if(WIN32)
    set_target_properties(Traktor PROPERTIES WIN32_EXECUTABLE TRUE)
    target_sources(Traktor PRIVATE icons/traktor.rc)
endif()

# Tests
option(BUILD_TESTING "Build test suite" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
