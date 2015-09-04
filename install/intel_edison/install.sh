#!/usr/bin/env sh
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DDAEMON=ON -DEDISON=ON ../../../
systemctl stop thinger.service
make thinger
make install
systemctl start thinger.service
systemctl enable thinger.service