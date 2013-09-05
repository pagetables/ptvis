#!/bin/sh
gcc dumper.c ../../common/arm/pt_printer.c -D__NO_PTPRINTER_MAIN -Wall -o rtdump
