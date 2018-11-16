#!/bin/bash

set -eu -o pipefail

brew update > /dev/null

brew install ccache qt5

export PATH="/usr/local/opt/qt/bin:$PATH"
export LDFLAGS="-L/usr/local/opt/qt/lib"
export CPPFLAGS="-I/usr/local/opt/qt/include"

cd ..

qmake
make -j$(sysctl -n hw.ncpu)
