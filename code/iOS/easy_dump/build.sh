#!/bin/sh
gcc dumper.c ../../common/arm/pt_printer.c -D__NO_PTPRINTER_MAIN -Wall -o localdump
/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/llvm-gcc-4.2 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS6.1.sdk/ -miphoneos-version-min=6.0 -fomit-frame-pointer -mthumb -arch armv7 dumper.c ../../common/arm/pt_printer.c -o ptwalk  -D__NO_PTPRINTER_MAIN

/Users/adc/justin_bieber/ldid -Sent.xml ptwalk 
