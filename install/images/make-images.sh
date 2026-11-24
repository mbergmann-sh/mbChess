#!/bin/sh
# Regenerates watermark.png (left side of the welcome and final pages: the
# About picture at half size) and logo.png (program icon). Needs ImageMagick and rsvg-convert.
cd "$(dirname "$0")"
convert ../../src/app/resources/images/about.jpg -resize 260x -strip watermark.png
# photo.png: the About picture for the welcome page (shown 160 px wide, 2x for HiDPI).
# PNG, not JPEG: the installer (QtIFW) has no JPEG image plugin, only PNG is built in.
convert ../../src/app/resources/images/about.jpg -resize 320x -strip photo.png
rsvg-convert -w 48 -h 48 ../../src/app/resources/icons/mbchess.svg > logo.png
