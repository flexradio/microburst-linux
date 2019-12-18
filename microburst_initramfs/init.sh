#!/bin/sh

# Mount proc and sys
mount -t proc proc /proc
mount -t sysfs sysfs /sys

/bin/busybox sleep 5

# Mount sdboot
mount -t vfat /dev/mmcblk0p0 /mnt/sdboot
echo "Boot filesystem mounted"

# Verify extraction of rootfs tarball works
mkdir -p /tmp/rootfs
if tar xf /mnt/sdboot/rootfs.tgz -C /tmp/rootfs/
then
      echo "rootfs successfully extracted"
else
      echo "rootfs extraction failed. Abort!"
      busybox reboot -d 0 -n -f
fi

# Create ext2 filesystem on mmc partition 2 and mount it
mke2fs -L root /dev/mmcblk0p1
mount /dev/mmcblk0p1 /mnt/sdroot
echo "New root filesystem created and mounted"

# Copy Extracted root filesystem from temp folder
echo "rootfs now being copied to root partition"
cp -ar /tmp/rootfs/* /mnt/sdroot/

echo "Sync() Flushing rootfs to disk"
sync

echo "New root filesystem extracted"

echo "Upgrade Complete"

if test -x /mnt/sdroot/bin/systemd ; then
	echo "Restarting radio.."
	rm /mnt/sdboot/ug-uImage
	rm /mnt/sdboot/rootfs.tgz
	rm /mnt/sdboot/manifest
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

