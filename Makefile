# Guitar Hero Linux build
#
#   make            build kernel + initramfs + ampkg
#   make ampkg      build the ampkg package manager binary
#   make backstage  build the Backstage game shell
#   make qemu       build and boot in QEMU (initramfs only)
#   make qemu-root  build and boot in QEMU (real rootfs, pivot)
#   make iso        build a bootable ISO
#   make check      run the complete 0.1 release gate
#   make release    validate and assemble the first-set QEMU kit
#   make clean      remove build artifacts

BUILD_DIR := build
OUT       := $(BUILD_DIR)/out

.PHONY: all ampkg backstage kernel busybox initramfs rootfs qemu qemu-root iso check release clean

all: kernel initramfs

ampkg:
	cd tools/ampkg && CGO_ENABLED=0 go build -trimpath -ldflags="-s -w" -o ampkg ./cmd/ampkg

backstage:
	bash scripts/build-backstage.sh

kernel:
	bash scripts/build-kernel.sh

busybox:
	bash scripts/build-busybox.sh

initramfs: busybox
	bash scripts/build-initramfs.sh

rootfs: busybox ampkg backstage
	bash scripts/build-rootfs.sh

qemu: all
	bash scripts/run.sh

qemu-root: all rootfs
	bash scripts/run-root.sh

iso: all
	bash scripts/make-iso.sh

check: all rootfs
	bash scripts/check-release.sh

release: check
	bash scripts/make-release.sh

clean:
	rm -rf $(BUILD_DIR)
