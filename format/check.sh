#!/bin/bash

# Prints any formatting mismatches

# Run this from the repo root, e.g.:
# format/check.sh

# Automatically fix formatting issues with:
# format/do.sh

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

$DIR/run-clang-format.py -r $(cat $DIR/targets.txt)
