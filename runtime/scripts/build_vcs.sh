#!/bin/bash

INCLUDE_SDIR=+incdir+$BEETHOVEN_PATH/build/hw/
export VCS_HOME=/all/eda/tools/synopsys/vcs/V-2023.12-SP2-2/
if [[ $MODE == 'BEHAVIORAL' ]]; then
	SLIST=${SLIST:-$BEETHOVEN_PATH/build/vcs_srcs.in}
elif [[ $MODE = 'POSTSYNTH' ]]; then
	INCLUDE_SDIR="$INCLUDE_SDIR +incdir+$SPACE/SMALL/Systemsd0_sysECore/data +incdir+$SPACE/SMALL/Systemsd0_sysMCore/data +incdir+$SPACE/SMALL/Systemsd0_sysGCore/data +incdir+$BEETHOVEN_PATH/build/hw"
	SLIST=${SLIST:-$BEETHOVEN_PATH/build/vcs_srcs_sans_cores.in}
	EXTRA_FILES=$TSMCN16_SCSVT_VLG
elif [[ $MODE = 'TOPSYNTH' ]]; then
	INCLUDE_SDIR="$INCLUDE_SDIR \
		+incdir+$SPACE/SMALL/Systemsd0_sysECore/data \
		+incdir+$SPACE/SMALL/Systemsd0_sysMCore/data \
		+incdir+$SPACE/SMALL/Systemsd0_sysGCore/data \
		+incdir+$SPACE/SMALL/ImplementationTop/data "
	SLIST=${SLIST:-$BEETHOVEN_PATH/build/top_level_vcs.in}
	M55_VLG=/all/works/camel/implementation/build_cm55_16ffc7/data/CM55-compile.v
	M0_VLG=/all/works/camel/implementation/build_cm0sys_16ffc2/data/CM0_SYS-compile.v
	# OLDIO_VLG=/all/eda/kits/tsmc/n16old/tphn16ffcllgv18e/All_Files/tphn16ffcllgv18e_090a_vlg/TSMCHOME/digital/Front_End/verilog/tphn16ffcllgv18e_090a/tphn16ffcllgv18e.v
	OLDIO_VLG=""
	CLOCK_GEN=/all/works/camel/implementation/build_vosc_16ffc/data/crg_vosc_slice-compile.v
	EXTRA_FILES="$TSMCN16_SCSVT_VLG $OLDIO_VLG $M55_VLG $M0_VLG $CLOCK_GEN"
else
	echo "PASS MODE AS EITHER 'POSTSYNTH' or 'BEHAVIORAL', got '$MODE'"
	VCS_DIR=/all/eda/tools/synopsys/vcs/V-2023.12-SP2-2/
	VCS_SHORT=tools/vcs/V-2023.12-SP2-2
	PLI_PATH=/all/eda/tools/synopsys/verdi/V-2023.12-SP2-2/share/PLI/VCS/linux64/pli.a
	SRAM_DIR=/all/eda/instances/rams
	export LD_LIBRARY_PATH=$HOME/INSTALL/lib:`pwd`:/home/chris/Composer-Runtime/DRAMsim3/:$HOME/INSTALL/lib64
	. /usr/share/Modules/init/sh; \
		        module load licenses/synopsys licenses/cadence licenses/siemens licenses/mentor-graphics special/unlimited-stack $VCS_SHORT; \
			        vcs -full64 -help
	exit
fi


VCS_DIR=/all/eda/tools/synopsys/vcs/V-2023.12-SP2-2/
VCS_SHORT=tools/vcs/V-2023.12-SP2-2
PLI_PATH=/all/eda/tools/synopsys/verdi/V-2023.12-SP2-2/share/PLI/VCS/linux64/pli.a
SRAM_DIR=/all/eda/instances/rams
cmake-chris .. -DDRAMSIM_CONFIG=custom_dram_configs/hyperram.ini -DTARGET=sim -DSIMULATOR=vcs -DVCS_INCLUDE_DIR=/all/eda/tools/synopsys/vcs/V-2023.12-SP2-2/include/
make -j
export LD_LIBRARY_PATH=$HOME/INSTALL/lib:`pwd`:/home/chris/Composer-Runtime/DRAMsim3/:$HOME/INSTALL/lib64
. /usr/share/Modules/init/sh; \
	module load licenses/synopsys licenses/cadence licenses/siemens licenses/mentor-graphics special/unlimited-stack $VCS_SHORT; \
	vcs +vcs+loopreport +v2k -kdb -timescale=1ps/1ps -debug_access $PLI_PATH -sverilog $INCLUDE_SDIR -full64 +vpi+1  \
	+define+CLOCK_PERIOD=500 -f $SLIST $EXTRA_FILES \
	$SRAM_DIR/SRAM_SP_HDE_LP_256X128M2B2/verilog/SRAM_SP_HDE_LP_256X128M2B2.v \
	$SRAM_DIR/SRAM_SP_HDE_LP_256X24M2B2/verilog/SRAM_SP_HDE_LP_256X24M2B2.v \
	$SRAM_DIR/SRAM_SP_HDE_LP_512X128M2B2/verilog/SRAM_SP_HDE_LP_512X128M2B2.v \
	$SRAM_DIR/SRAM_SP_HDE_LP_512X19M4B2/verilog/SRAM_SP_HDE_LP_512X19M4B2.v \
	$SRAM_DIR/SRAM_SP_HDE_LP_1024X128M2B2/verilog/SRAM_SP_HDE_LP_1024X128M2B2.v \
	$SRAM_DIR/SRAM_SP_HDE_LP_1024X32M4B2/verilog/SRAM_SP_HDE_LP_1024X32M4B2.v \
	$SRAM_DIR/SRAM_SP_HDE_LP_2048X128M4B2/verilog/SRAM_SP_HDE_LP_2048X128M4B2.v \
	$SRAM_DIR/SRAM_SP_HDE_LP_4096X128M4B8/verilog/SRAM_SP_HDE_LP_4096X128M4B8.v \
	$SRAM_DIR/SRAM_SP_HDE_LP_4096X32M4B4/verilog/SRAM_SP_HDE_LP_4096X32M4B4.v \
	$SRAM_DIR/SRAM_SP_HDE_LP_16384X64M8B8/verilog/SRAM_SP_HDE_LP_16384X64M8B8.v \
	$SRAM_DIR/SRAM_SP_HDE_LP_16384X64M16B8/verilog/SRAM_SP_HDE_LP_16384X64M16B8.v \
	$SRAM_DIR/SRAM_SP_HDE_LP_16384X32M16B4/verilog/SRAM_SP_HDE_LP_16384X32M16B4.v \
	+define+ARM_UD_MODEL=1 +error+100 \
	-P ../scripts/tab.tab \
	-L./ \
	-L$HOME/INSTALL/lib64 \
        libBeethovenRuntime.so ../DRAMsim3/libdramsim3.so -lrt -L/usr/local/lib64 -L/home/chris/INSTALL/lib \
	-L$HOME/INSTALL/lib -lc++ -lc++abi -lLTO \
	-lbeethoven -lpthread -lc -CFLAGS \
	-I$SDIR $BEETHOVEN_PATH/build/hw/BeethovenTopVCSHarness.v -o BeethovenTop 

