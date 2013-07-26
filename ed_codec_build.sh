#! /bin/bash

make -j8 uImage
make -j8 modules
make INSTALL_MOD_PATH=/nfsroots/microburst modules_install
cd ti-dsp/syslink_2_21_00_03/
make syslink
make install
cd ../../
