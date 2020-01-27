#! /bin/bash -x

#Setup Cross-Compile Prefix 
export CROSS_COMPILE=/opt/ti/CodeSourcery/Sourcery_G++_Lite/bin/arm-none-linux-gnueabi-
#Set target architecture to ARM
export ARCH=arm

#Copy the configuration file to the build configuration
cp microburst_updater_config .config
make -j8 uImage

#Copy to to correct location in microburst-uboot
cp /src/flex/microburst-linux/arch/arm/boot/uImage /src/flex/microburst-uboot/ProductionSD/boot/ug-uImage
cp arch/arm/boot/uImage ./ug-uImage
sync
