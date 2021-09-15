#!/usr/bin/env bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DDAEMON=OFF ../
make thinger
./thinger