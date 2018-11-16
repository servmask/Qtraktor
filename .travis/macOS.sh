#!/bin/bash

set -eu -o pipefail

brew update > /dev/null

brew install ccache qt5

export PATH="/usr/local/opt/qt/bin:$PATH"
export LDFLAGS="-L/usr/local/opt/qt/lib"
export CPPFLAGS="-I/usr/local/opt/qt/include"

cd $TRAVIS_BUILD_DIR

qmake Qtraktor.pro
make -j$(sysctl -n hw.ncpu)

# add dependencies
macdeployqt Traktor.app
