#!/bin/bash

# Automatically fixes formatting issues:

# Run this from the repo root, e.g.:
# format/do.sh

# To just check for formatting issues (without modifying files) run:
# format/check.sh

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

$DIR/run-clang-format.py -i -r $(cat $DIR/targets.txt)
