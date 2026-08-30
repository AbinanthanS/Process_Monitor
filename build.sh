#!/usr/bin/env bash
set -e
echo "Compiling Linux Process Monitor..."
g++ -std=c++17 -Wall -Wextra -O2 -pthread -Iinclude -Iinclude/core -Iinclude/collectors -Iinclude/ui src/*.cpp src/*/*.cpp -o monitor
echo "Build complete! Run with: ./monitor"
