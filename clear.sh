#!/bin/bash
set -e

ROOT_DIR="/home/young/Project/miniparser"
BUILD_DIR="$ROOT_DIR/build"
TARGET_DIR="$ROOT_DIR/target"
TARGET_EXE="$TARGET_DIR/miniparser"
INPUT_FILE="$ROOT_DIR/tests/sample.dyd"
PRO_FILE="$ROOT_DIR/tests/sample.pro"
ERR_FILE="$ROOT_DIR/tests/sample.err"
VAR_FILE="$ROOT_DIR/tests/sample.var"

rm -rf "$BUILD_DIR" "$TARGET_DIR"

rm $PRO_FILE $ERR_FILE $VAR_FILE