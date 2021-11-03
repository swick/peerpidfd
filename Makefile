KERNEL_SOURCE_PATH := ../../linux
KERNEL_BUILD_DIR := $(CURDIR)/kernel-build

.PHONY: all config

all: $(KERNEL_BUILD_DIR)/arch/x86/boot/bzImage $(KERNEL_BUILD_DIR)/usr/initramfs_data.cpio
	qemu-system-x86_64 -nographic -append console=ttyS0 \
		-kernel $(KERNEL_BUILD_DIR)/arch/x86/boot/bzImage \
		-initrd $(KERNEL_BUILD_DIR)/usr/initramfs_data.cpio

config: $(KERNEL_BUILD_DIR)/.config


$(KERNEL_BUILD_DIR)/.config: linux_defconfig
	cd $(KERNEL_SOURCE_PATH) && \
		make O=$(KERNEL_BUILD_DIR) defconfig && \
		./scripts/kconfig/merge_config.sh -O $(KERNEL_BUILD_DIR) $(KERNEL_BUILD_DIR)/.config $(CURDIR)/linux_defconfig

$(KERNEL_BUILD_DIR)/arch/x86/boot/bzImage: $(KERNEL_BUILD_DIR)/.config initramfs.list src/init src/peerpidfd src/evil
	cd $(KERNEL_SOURCE_PATH) && \
		make O=$(KERNEL_BUILD_DIR) -j9

%: %.c
	cc -static -o $@ $<

