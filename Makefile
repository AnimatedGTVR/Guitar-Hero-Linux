# Guitar Hero Linux build
#
#   make            build kernel + initramfs
#   make qemu       build and boot in QEMU
#   make iso        build a bootable ISO
#   make clean      remove build artifacts

BUILD_DIR := build
OUT       := $(BUILD_DIR)/out

.PHONY: all kernel initramfs qemu iso clean

all: kernel initramfs

kernel:
	bash scripts/build-kernel.sh

initramfs:
	bash scripts/build-initramfs.sh

qemu: all
	bash scripts/run.sh

iso: all
	bash scripts/make-iso.sh

clean:
	rm -rf $(BUILD_DIR)
