#!/usr/bin/env bash
set -e
echo "Compiling Linux Process Monitor..."
g++ -std=c++17 -Wall -Wextra -O2 -pthread terminal.cpp render_buffer.cpp cpu.cpp memory.cpp process.cpp app.cpp main.cpp -o monitor
echo "Build complete! Run with: ./monitor"
