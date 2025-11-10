#!/bin/bash
set -e

ROOT_DIR="/home/young/Project/miniparser"
BUILD_DIR="$ROOT_DIR/build"
TARGET_DIR="$ROOT_DIR/target"
TARGET_EXE="$TARGET_DIR/miniparser"
INPUT_FILE="$ROOT_DIR/tests/sample1.dyd"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake ..
make

"$TARGET_EXE" "$INPUT_FILE"