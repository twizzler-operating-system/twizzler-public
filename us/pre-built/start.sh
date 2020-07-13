#!/bin/sh

if [[ "$NV" == "keep" ]]; 
	echo Keeping old pmem.img
else
	echo Overwriting old pmem.img
	rm pmem.img 
	touch pmem.img
	truncate -s 4G pmem.img
fi

qemu-system-x86_64 \
	-cpu host,migratable=false,host-cache-info=true,host-phys-bits \
	-machine q35,nvdimm,kernel-irqchip=split \
	-device intel-iommu,intremap=on,aw-bits=48,x-scalable-mode=true \
	-m 1024,slots=2,maxmem=8G \
	-object memory-backend-file,id=mem1,share=on,mem-path=projects/x86_64/build/pmem.img,size=4G \
	-device nvdimm,id=nvdimm1,memdev=mem1 \
	-enable-kvm \
	-smp 4 \
	-cdrom projects/x86_64/build/boot.iso \
	-drive file=projects/x86_64/build/us/nvme.img,if=none,id=D22 \
	-device nvme,drive=D22,serial=1234 -serial stdio

