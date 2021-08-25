#!/bin/bash

cd ./os161-base-2.0.3/kern/compile
rm -r SUCHVM
cd ../conf
./config SUCHVM
cd ../compile/SUCHVM
bmake depend
bmake
bmake install

cd ../../../../root/
gnome-terminal -- sys161 -w kernel-SUCHVM
sleep 0.5
ddd --debugger mips-harvard-os161-gdb kernel-SUCHVM