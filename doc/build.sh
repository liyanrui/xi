#!/bin/bash
xi -c ctx.conf -w -o xi-doc.tex ../xi.xi \
 && sed -i "s/ESC_AT/@/g; s/ESC_SHARP/#/g" xi-doc.tex \
 && context xi-doc.tex
