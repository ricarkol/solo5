sudo grub-bhyve -M 128M ping < grub.in

sudo bhyve -A -H -P -s 0:0,hostbridge -s 1:0,lpc -s 2:0,virtio-net,tap1 -l com1,stdio -m 128M ping
