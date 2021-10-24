%: %.c
	cc -static -o $@ $<

initrd: initrd.gz
initrd.gz: init peerpidfd evil
	echo $^ | sed -e 's/ /\n/g'  | cpio -H newc -o | gzip > initrd.gz

../linux/arch/x86/boot/bzImage:
	cd ../linux && make defconfig && make -j9

all: initrd.gz ../linux/arch/x86/boot/bzImage

.PHONY run:
	qemu-system-x86_64 -nographic -append console=ttyS0 -kernel ../linux/arch/x86/boot/bzImage -initrd initrd.gz 
