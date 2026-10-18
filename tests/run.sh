#!/bin/sh
set -e
cd "$(dirname "$0")/.."
g++ -std=c++17 -Wall -pthread -o /tmp/ss_reverb tests/reverb_test.cpp && /tmp/ss_reverb
