TARGET=${ANDROID_NDK}/toolchains/arm-linux-androideabi-4.4.3/prebuilt/linux-x86/bin/arm-linux-androideabi
CFLAGS=-I ${ANDROID_NDK}/platforms/android-14/arch-arm/usr/include -D__NO_PTPRINTER_MAIN -ggdb
LFLAGS=-Wl,--dynamic-linker=/system/bin/linker -Wl,-rpath=/system/lib -L$(PLATFORM)/usr/lib -nostdlib
PLATFORM?=${ANDROID_NDK}/platforms/android-14/arch-arm
INSTALL_PATH?=/data/local/tmp/pgtcollect_elev8
PHYS_START?=0x80000000L
CFLAGS+=-DPHYS_START=$(PHYS_START)

.PHONY: clean install

pgtcollect_elev8: pgtcollect_elev8.c ../../common/arm/pt_printer.c elev8or.c
	@[ -n "${ANDROID_NDK}" ] || ( echo 'Please set ANDROID_NDK environment variable to the root of the latest Android NDK!' 2>&1; exit 1 )
	@[ -n "${ANDROID_NDK}" ] || ( echo 'Please set ANDROID__KERNEL_INCLUDE environment variable to the ./include/ of your kernel checkout!' 2>&1;exit 1 )
	$(TARGET)-gcc $(CFLAGS) -I${ANDROID_KERNEL_INCLUDE} -march=armv7-a $(LFLAGS) $(PLATFORM)/usr/lib/crtbegin_dynamic.o $(PLATFORM)/usr/lib/crtend_android.o -Os -mthumb -o $@ $^ -lc -lgcc
	#$(TARGET)-strip -S $@

install: pgtcollect_elev8
	adb push $< $(INSTALL_PATH)

clean:
	rm -f pgtcollect_elev8
