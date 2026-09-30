#!/bin/bash
echo "clean cmake generated file and external build"

rm -rf CMakeCache.txt
rm -rf CMakeFiles/
rm -f Makefile
rm -f cmake_install.cmake

rm -rf External/
rm -rf _deps

rm -rf build

rm -rf bin
rm -rf RTypeClient
rm -rf RTypeServer

echo "clean finished"