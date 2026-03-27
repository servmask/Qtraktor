QT += core testlib network
QT -= gui

# We need widgets for some components being tested
QT += widgets

CONFIG += c++11 console testcase sdk_no_version_check
CONFIG -= app_bundle

TARGET = tst_qtraktor
TEMPLATE = app

INCLUDEPATH += ..
INCLUDEPATH += ../vendor/bzip2-1.0.8

# Link same libraries as the main project
unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += openssl
}
unix:!android: LIBS += -lz
macx: LIBS += -lz

win32-g++ {
    LIBS += -lz
    OPENSSL_DIR = $$(OPENSSL_DIR)
    !isEmpty(OPENSSL_DIR) {
        INCLUDEPATH += $$OPENSSL_DIR/include
        LIBS += -L$$OPENSSL_DIR/lib -lssl -lcrypto
    }
}

win32-msvc {
    LIBS += zlib.lib
    OPENSSL_DIR = $$(OPENSSL_DIR)
    !isEmpty(OPENSSL_DIR) {
        INCLUDEPATH += $$OPENSSL_DIR/include
        LIBS += -L$$OPENSSL_DIR/lib -llibssl -llibcrypto
    }
}

QMAKE_CFLAGS += -w

# bzip2
BZIP2_DIR = $$PWD/../vendor/bzip2-1.0.8
INCLUDEPATH += $$BZIP2_DIR

SOURCES += \
    tst_main.cpp \
    tst_backupfile.cpp \
    tst_cryptoutils_streaming.cpp \
    tst_extractionworker.cpp \
    tst_qsettings.cpp \
    ../backupfile.cpp \
    ../cryptoutils.cpp \
    ../extractionworker.cpp \
    $$BZIP2_DIR/blocksort.c \
    $$BZIP2_DIR/huffman.c \
    $$BZIP2_DIR/crctable.c \
    $$BZIP2_DIR/randtable.c \
    $$BZIP2_DIR/compress.c \
    $$BZIP2_DIR/decompress.c \
    $$BZIP2_DIR/bzlib.c

HEADERS += \
    ../backupfile.h \
    ../cryptoutils.h \
    ../extractionworker.h
