#!/bin/bash
# script for meson custom_target rule

echo "$1 markdown to $2 html"
pandoc -f markdown -s $1 -o $2

