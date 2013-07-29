## THIS IS A GENERATED FILE -- DO NOT EDIT
.configuro: .libraries,e674 linker.cmd \
  package/cfg/Dsp_pe674.oe674 \

linker.cmd: package/cfg/Dsp_pe674.xdl
	$(SED) 's"^\"\(package/cfg/Dsp_pe674cfg.cmd\)\"$""\"/src/flex/microburst-linux/ti-dsp/syslink_2_21_00_03/examples/ex07_gatemp/dsp/configuro/\1\""' package/cfg/Dsp_pe674.xdl > $@
