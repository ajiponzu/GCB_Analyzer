#!/bin/sh

cd build
echo "+++++++  build start..........."
# cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja -j$(nproc)
echo "+++++++  ........build finished"
echo "+++++++  ...................run"
./gcb-analyzer
