#!/bin/sh

cd build
rm -rf *
# cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja -j$(nproc)