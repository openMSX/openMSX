#!/bin/bash

sdasz80 -g -l -c -o omsxctl.rel omsxctl.asm
sdcc -mz80 --nostdinc --no-std-crt0 --code-loc 0x0100 -o omsxctl.hex omsxctl.rel
hex2bin -e com omsxctl.hex

