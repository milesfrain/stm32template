#!/usr/bin/env bash

# This script is intended to be run in an empty tmux window
# For example:
#  tmux
#  ./launch.sh

# rebuild apps
make all
pushd ../monitor; make all; popd

# todo - copy logfiles to backup dir

# clear all logfile content
truncate -s 0 *.bin

# open panes for all monitors
# todo, last monitor needs an option to suppress sequence errors when reading all.bin
tmux \
  split-window "tail -c +1 -f in.bin | ../monitor/monitor" \; \
  split-window "tail -c +1 -f out.bin | ../monitor/monitor" \; \
  split-window "tail -c +1 -f all.bin | ../monitor/monitor" \; \
  select-pane -t 1 \; \
  select-layout even-vertical

# launch commander app
./commander

# Should be able to press 'q' key then ctrl-c 3 more times
# to get back to original window.

# Reorder pane selection to make manual shutdown easier
tmux \
  select-pane -t 2 \; \
  select-pane -t 3 \; \
  select-pane -t 4 \; \
