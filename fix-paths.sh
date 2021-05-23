#!/bin/bash

# The IDE insists on converting absolute paths to relative paths.
# This is dumb and breaks the build for any project that is located with deeper nesting.
# For more details, see:
# https://community.st.com/s/question/0D53W00000Ioh2LSAR/stm32cubeide-uses-relative-paths-when-linking-to-firmware-at-an-absolute-path

find . -name .project | xargs sed -i 's|$%7BPARENT-[[:digit:]]\+-PROJECT_LOC%7D/usr/local/share|/usr/local/share|g'

