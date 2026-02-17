#-------------------------------------------------
#
# Project created by QtCreator 2018-11-06T19:58:14
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Traktor
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11 sdk_no_version_check

# zlib library (for decompression)
# bzip2 is bundled in vendor/bzip2-1.0.8 and built as part of the project

win32-g++ {
    LIBS += -lz
    # OpenSSL for MinGW: uses OPENSSL_DIR environment variable
    OPENSSL_DIR = $$(OPENSSL_DIR)
    !isEmpty(OPENSSL_DIR) {
        INCLUDEPATH += $$OPENSSL_DIR/include
        LIBS += -L$$OPENSSL_DIR/lib -lssl -lcrypto
    }
}

win32-msvc {
    LIBS += zlib.lib
    # OpenSSL for MSVC: uses OPENSSL_DIR environment variable
    OPENSSL_DIR = $$(OPENSSL_DIR)
    !isEmpty(OPENSSL_DIR) {
        INCLUDEPATH += $$OPENSSL_DIR/include
        LIBS += -L$$OPENSSL_DIR/lib -llibssl -llibcrypto
    }
}
unix:!android {
    LIBS += -lz
}
macx {
    LIBS += -lz
}

# OpenSSL via pkg-config (macOS, Linux)
unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += openssl
}

# Suppress C compiler warnings (only affects vendored bzip2 .c files;
# all project code is C++ and uses QMAKE_CXXFLAGS instead)
QMAKE_CFLAGS += -w

# bzip2 source package
BZIP2_DIR = $$PWD/vendor/bzip2-1.0.8
INCLUDEPATH += $$BZIP2_DIR

SOURCES += \
        main.cpp \
        mainwindow.cpp \
        backupfile.cpp \
        cryptoutils.cpp \
        passworddialog.cpp \
        appdelegate.cpp \
        dropoverlay.cpp \
        $$BZIP2_DIR/blocksort.c \
        $$BZIP2_DIR/huffman.c \
        $$BZIP2_DIR/crctable.c \
        $$BZIP2_DIR/randtable.c \
        $$BZIP2_DIR/compress.c \
        $$BZIP2_DIR/decompress.c \
        $$BZIP2_DIR/bzlib.c

HEADERS += \
        mainwindow.h \
        backupfile.h \
        cryptoutils.h \
        passworddialog.h \
        appdelegate.h \
        dropoverlay.h

FORMS += \
        mainwindow.ui

RC_ICONS = icons/traktor.ico
ICON = icons/traktor.icns

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

QMAKE_TARGET_COMPANY = "ServMask, Inc."
QMAKE_TARGET_PRODUCT = "Traktor"
QMAKE_TARGET_DESCRIPTION = "WPRESS Extractor"
QMAKE_TARGET_COPYRIGHT = "ServMask, Inc."