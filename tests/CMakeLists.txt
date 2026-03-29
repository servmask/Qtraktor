find_package(Qt6 REQUIRED COMPONENTS Test)

# Static library to share compiled sources between app and tests.
# Targets bzip2, OpenSSL, ZLIB are inherited from the parent scope.
add_library(traktor_lib STATIC
    ../src/backupfile.cpp
    ../src/cryptoutils.cpp
    ../src/extractionworker.cpp
    ../src/clihandler.cpp
    ../src/mcpserver.cpp
    ../src/installcli.cpp
    ../src/agentconfig.cpp
    ../src/backupfile.h
    ../src/cryptoutils.h
    ../src/extractionworker.h
    ../src/clihandler.h
    ../src/mcpserver.h
    ../src/installcli.h
    ../src/agentconfig.h
)

target_include_directories(traktor_lib PUBLIC ../src)

target_link_libraries(traktor_lib PUBLIC
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    bzip2
)

qt_add_executable(tst_qtraktor
    tst_main.cpp
    tst_backupfile.cpp
    tst_cryptoutils_streaming.cpp
    tst_extractionworker.cpp
    tst_qsettings.cpp
    tst_fuzz.cpp
    tst_cli.cpp
)

target_link_libraries(tst_qtraktor PRIVATE traktor_lib Qt6::Test)

# Copy test fixtures to the build directory so tests find them
file(COPY fixtures DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME QtraktorTests COMMAND tst_qtraktor)
