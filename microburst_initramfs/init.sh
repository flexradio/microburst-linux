#!/bin/busybox sh

set -x

restart_radio() {
  echo "Restarting radio.."
  rm -f /mnt/sdboot/ug-uImage /mnt/sdboot/rootfs.tgz /mnt/sdboot/manifest /mnt/sdboot/firmware_*.tar
  sync
  umount /sys /proc /mnt/sdboot
  busybox reboot -d 0 -n -f
}

/bin/busybox --install -s

# Mount proc and sys
mount -t proc proc /proc
mount -t sysfs sysfs /sys

/bin/busybox sleep 5

# Mount sdboot
mount -t vfat /dev/mmcblk0p0 /mnt/sdboot
echo "Boot filesystem mounted"

# Verify extraction of rootfs tarball works
if ! tar -xf /mnt/sdboot/rootfs.tgz  -O > /dev/null; then
  echo "rootfs extraction failed. Abort!"
  restart_radio
fi

# Verify the firmware bundle if it's there
for f in /mnt/sdboot/firmware_*.tar; do
  if [[ -f "$f" ]]; then
    echo "Found firmware bundle, testing integrity."
    if ! tar -xf /mnt/sdboot/firmware_*.tar -O > /dev/null; then
      echo "Firmware bundle is corrupt!"
      restart_radio
    fi
  else
    echo "No firmware bundle present, skipping for now."
  fi
  break
done

# Create ext2 filesystem on mmc partition 2 and mount it
mke2fs -L root /dev/mmcblk0p1
mount /dev/mmcblk0p1 /mnt/sdroot
echo "New root filesystem created and mounted"

# Copy Extracted root filesystem from temp folder
echo "rootfs now being extracted to root partition"
tar -C /mnt/sdroot/ -xf /mnt/sdboot/rootfs.tgz
tar -C /mnt/sdroot/ -xf /mnt/sdboot/firmware_*.tar

echo "New root filesystem extracted"
echo "Upgrade Complete"

restart_radio

# If you get here, something bad happened.  

# Kill kernel printk messages
echo 0 > /proc/sys/kernel/printk

# Drop to shell for repairs
exec sh

