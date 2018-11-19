#!/bin/bash

set -eu -o pipefail

brew update > /dev/null

brew install qt5

brew tap yani-/homebrew-qtifw
brew install qt-ifw

export PATH="/usr/local/opt/qt/bin:/usr/local/opt/qt-ifw/bin:$PATH"
export LDFLAGS="-L/usr/local/opt/qt/lib"
export CPPFLAGS="-I/usr/local/opt/qt/include"

cd $TRAVIS_BUILD_DIR

qmake Qtraktor.pro
make -j$(sysctl -n hw.ncpu)

# add dependencies
macdeployqt Traktor.app

mkdir packages/com.servmask.traktor/data

cp -r Traktor.app packages/com.servmask.traktor/data

binarycreator -c config/config.xml -p packages Traktor
