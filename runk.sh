#!/bin/bash


qemu-system-x86_64 \
-kernel /home/mateusz/zso/linux-5.5.5/arch/x86/boot/bzImage \
-drive file=zso2020.qcow2,if=none,id=drive0 \
-append root=/dev/sda1 \
-device virtio-scsi-pci,id=scsi0 \
-device scsi-hd,bus=scsi0.0,drive=drive0 \
-enable-kvm \
-smp 4 \
-net nic,model=virtio -net user \
-net user,hostfwd=tcp::2222-:22 \
-m 1G -balloon virtio \
-fsdev local,id=hshare,path=hshare/,security_model=none \
-device virtio-9p-pci,fsdev=hshare,mount_tag=hshare \
-chardev stdio,id=cons,signal=off -device virtio-serial-pci \
-device virtconsole,chardev=cons \
-soundhw hda \
-usb -device usb-mouse
