#!/bin/bash

convert kamonegi_shadow.png -resize 256x256 icon_256.png
convert kamonegi_shadow.png -resize 128x128 icon_128.png
convert kamonegi.png        -resize 64x64   icon_64.png
convert kamonegi.png        -resize 32x32   icon_32.png
convert kamonegi.png        -resize 16x16   icon_16.png

convert icon_16.png icon_32.png icon_64.png icon_128.png icon_256.png kamonegi_shadow.png icon.ico
rm icon_*.png
