KERNEL_SOURCE_PATH := ../linux
KERNEL_BRANCH := sopeerpidfd
KERNEL_SELFTEST_BUILD_DIR := $(CURDIR)/kernel-build

.PHONY all: build
	qemu-system-x86_64 -nographic -append console=ttyS0 -kernel $(KERNEL_SELFTEST_BUILD_DIR)/arch/x86/boot/bzImage -initrd initrd.gz

build: initrd.gz $(KERNEL_SELFTEST_BUILD_DIR)/arch/x86/boot/bzImage

%: %.c
	cc -static -o $@ $<

initrd.gz: init peerpidfd evil
	echo $^ | sed -e 's/ /\n/g'  | cpio -H newc -o | gzip > initrd.gz

$(KERNEL_SELFTEST_BUILD_DIR)/arch/x86/boot/bzImage:
	cd $(KERNEL_SOURCE_PATH) && git checkout $(KERNEL_BRANCH) && make O=$(KERNEL_SELFTEST_BUILD_DIR) defconfig && make O=$(KERNEL_SELFTEST_BUILD_DIR) -j9
