#!/bin/sh
set -e
cd "$(dirname "$0")/.."
g++ -std=c++17 -O2 -Wall -Isrc -o /tmp/ss_measure tests/measure.cpp && /tmp/ss_measure
