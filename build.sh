#!/bin/bash
echo "build start"

cmake -B build
cd build && make

echo "build finished"