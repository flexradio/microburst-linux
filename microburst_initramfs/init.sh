#!/bin/sh

# Mount proc and sys
mount -t proc proc /proc
mount -t sysfs sysfs /sys

/bin/busybox sleep 5

# Mount sdboot
mount -t vfat /dev/mmcblk0p0 /mnt/sdboot

echo "Boot filesystem mounted"

# Create ext2 filesystem on mmc partition 2 and mount it
mke2fs -L root /dev/mmcblk0p1
mount /dev/mmcblk0p1 /mnt/sdroot

echo "New root filesystem created and mounted"

# Extract root filesystem from tarball
tar zxf /mnt/sdboot/rootfs.tgz -C /mnt/sdroot/
sync

echo "New root filesystem extracted"

echo "Upgrade Complete"

if test -x /mnt/sdroot/bin/systemd ; then
	echo "Restarting radio.."
	rm /mnt/sdboot/ug-uImage
	rm /mnt/sdboot/rootfs.tgz
	sync
	umount /sys /proc /mnt/sdboot
#	exec switch_root /mnt/sdroot /bin/systemd
        busybox reboot -d 0 -n -f
fi
	
# If you get here, something bad happened.  

# Kill kernel printk messages
echo 0 > /proc/sys/kernel/printk

# Drop to shell for repairs
exec sh

