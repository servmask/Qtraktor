#!/bin/bash

set -eu -o pipefail

brew update > /dev/null

brew install ccache qt5

cd ..
qmake
make -j$(sysctl -n hw.ncpu)
