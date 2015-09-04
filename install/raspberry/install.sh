#!/usr/bin/env bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DDAEMON=ON -DRASPBERRY=ON ../../../
sudo service thinger stop
make thinger
sudo make install
sudo chmod +x /etc/init.d/thinger
sudo update-rc.d thinger defaults
sudo service thinger start