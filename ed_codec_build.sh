#! /bin/bash
export CROSS_COMPILE=arm-linux-gnueabi-
export ARCH=arm
make -j8 uImage
make -j8 modules
make INSTALL_MOD_PATH=/nfsroots/microburst modules_install
cd ti-dsp/syslink_2_21_02_10/
make syslink
make install
cd ../../
depmod -b /nfsroots/microburst 2.6.37-0.1-microburst+
sync
