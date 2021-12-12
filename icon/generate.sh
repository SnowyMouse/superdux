#!/bin/bash

convert superdux_shadow.png -resize 256x256 icon_256.png
convert superdux_shadow.png -resize 128x128 icon_128.png
convert superdux.png        -resize 64x64   icon_64.png
convert superdux.png        -resize 32x32   icon_32.png
convert superdux.png        -resize 16x16   icon_16.png

convert icon_256.png icon_128.png icon_64.png icon_32.png icon_16.png icon.ico
rm icon_*.png
