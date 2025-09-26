PLATFORM = discrete
TARGET = sim
DEVICE = kria
SIMULATOR ?= icarus

SUDO ?= sudo
PREFIX ?= /usr/local/
BUILD_TYPE ?= DEBUG

VERILATOR_INC = /opt/homebrew/share/verilator/include/

ifeq ($(PLATFORM),discrete)
SW_DEF = -DDISCRETE
HANDLE_O = src/fpga_handle_impl/fpga_handle_discrete.o
else
ifeq ($(PLATFORM),kria)
SW_DEF = -DKria
HANDLE_O = src/fpga_handle_impl/fpga_handle_kria.o
else
HANDLE_O = src/fpga_handle_impl/fpga_handle_baremetal.o
SW_DEF = -DBAREMETAL
endif
endif

CXX = c++

.PHONY: default
ifeq ($(SIMULATOR),verilator)
default: BeethovenSim
endif
ifeq ($(SIMULATOR),vcs)
default: BeethovenSim
endif
ifeq ($(SIMULATOR),icarus)
default: sim_icarus
endif

.PHONY: install_swlib
install_swlib:
	mkdir -p build && \
		cd build && \
		cmake .. -DPLATFORM=$(PLATFORM) -DCMAKE_INSTALL_PREFIX=$(PREFIX) && \
		make -j && \
		$(SUDO) make install

CXX_FLAGS = --std=c++17 -fPIC \
	    -I${BEETHOVEN_PATH}/build/ \
	    -I/usr/local/include/iverilog/ \
	    -Iinclude/ \
	    -I/usr/local/include/beethoven \
	    -IDRAMsim3/src/ \
	    -IDRAMsim3/ext/headers/ \
	    -Iinclude -Iruntime/include -Iruntime/DRAMsim3/src \
	    -Iruntime/DRAMsim3/ext/headers -I$(BEETHOVEN_PATH)/build/
PWD = $(shell pwd)

UNAME_S:=$(shell uname -s)
LD_FLAGS = -L/usr/local/lib/ivl/ -Lruntime/DRAMsim3 -ldramsim3 

ifeq ($(UNAME_S),Linux)
	LIB_EXPORT = export LD_LIBRARY_PATH=/usr/local/lib64:runtime/DRAMsim3/
endif

ifeq ($(UNAME_S),Darwin)
	LD_FLAGS += -rpath /usr/local/lib -undefined suppress
	LIB_EXPORT="export LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):runtime/DRAMsim3/"
endif

VPI_LOC = /usr/local/lib/ivl
VPI_FLAGS = $(VPI_LOC)/system.vpi
VERILOG_FLAGS = -DCLOCK_PERIOD=4 -I${BEETHOVEN_PATH}/build/hw
VERILOG_SRCS = $(shell cat ${BEETHOVEN_PATH}/build/vcs_srcs.in) 

# DEBUG FLAGS
CXX_FLAGS += -O0 -g3 
# RELEASE FLAGS
#CXX_FLAGS += -O2
FRONTBUS=axi

libdramsim3.so:
	$(MAKE) -C runtime/DRAMsim3/ -j
	cp runtime/DRAMsim3/libdramsim3.so .

SRCS = 	runtime/src/data_server.o \
	runtime/src/cmd_server.o \
	runtime/src/mmio.o \
	runtime/src/sim/axi/front_bus_ctrl_axi.o \
	runtime/src/sim/tick.o \
	runtime/src/sim/mem_ctrl.o \
	runtime/src/sim/DataWrapper.o \
	src/verilator_server.o \
	src/rocc_response.o \
	src/util.o \
	src/response_handle.o \
	src/allocator/alloc.o \
	src/rocc_cmd.o \
	$(HANDLE_O)

ifeq ($(SIMULATOR),icarus)
SIMULATOR_BACKEND=vpi
DEPS = sim_BeethovenRuntime.vpi beethoven.vvp

VERILOG_FLAGS += -DICARUS
VERILOG_SRCS += ${BEETHOVEN_PATH}/build/hw/BeethovenTopVCSHarness.v
CXX_FLAGS += -DSIM=vcs
CXX_DEPS = 
endif

ifeq ($(SIMULATOR),vcs)
SIMULATOR_BACKEND=vpi
DEPS =
CXX_FLAGS += -DSIM=vcs -I$(VCS_HOME)/include
CXX_DEPS = 
VERILOG_SRCS += ${BEETHOVEN_PATH}/build/hw/BeethovenTopVCSHarness.v
endif


ifeq ($(SIMULATOR),verilator)
SIMULATOR_BACKEND=verilator
DEPS = obj_dir/VBeethovenTop.cpp
CXX_FLAGS += -Iobj_dir -I$(VERILATOR_INC) -DVERILATOR
VERILATOR_SRCS = $(shell ls obj_dir/*.cpp)
VERILATOR_DEPS = $(patsubst %.c, %.o, $(VERILATOR_SRCS))
CXX_FLAGS += -DSIM=verilator

# verilator specific
USER_CPPFLAGS = -std=c++17
SRCS += runtime/src/sim/verilator.o
endif

SRCS += runtime/src/sim/axi/${SIMULATOR_BACKEND}_axi_frontend.o

lib_beethoven.o: ${BEETHOVEN_PATH}/build/beethoven_hardware.cc ${BEETHOVEN_PATH}/build/beethoven_hardware.h
	$(CXX) -c $(CXX_FLAGS) -o$@ ${BEETHOVEN_PATH}/build/beethoven_hardware.cc

runtime/src/%.o: runtime/src/%.cc $(CXX_DEPS)
	$(CXX) -c ${CXX_FLAGS} -o$@ $<

src/%.o: src/%.cc 
	$(CXX) -c ${CXX_FLAGS} $(SW_DEF) -o$@ $^

# for icarus - requires .vpi ending
sim_BeethovenRuntime.vpi: $(SRCS) lib_beethoven.o libdramsim3.so
	$(CXX) -shared $(LD_FLAGS) -o$@ $^

# for VCS, allows standard .so ending
libBeethovenRuntime.so: $(SRCS) lib_beethoven.o libdramsim3.so
	$(CXX) -shared $(LD_FLAGS) -o$@ $^



TESTS = bin/alloc_sizes bin/merge_sort

bin/%: test/%.cc $(SRCS) libdramsim3.so
	mkdir -p bin
	$(CXX) -c ${CXX_FLAGS} $< -o $(basename $<)
	$(CXX) $(LD_FLAGS) -o $@ $(SRCS) $(basename $<)

test: $(TESTS)

lint: $(VERILOG_SRCS)
	verilator --lint-only -Wno-timescalemod -Wall +incdir+$(BEETHOVEN_PATH)/build/hw -top BeethovenTop $(VERILOG_SRCS)


############### VERILATOR ONLY ###################
ifeq ($(SIMULATOR),verilator)

VERILATOR_DISABLE_WARN=-Wno-ascrange -Wno-pinmissing -Wno-widthexpand

.PHONY: verilate lint
verilate: $(VERILOG_SRCS)
	verilator --hierarchical $(VERILATOR_DISABLE_WARN) --cc --top BeethovenTop --trace-fst +incdir+$(BEETHOVEN_PATH)/build/hw $(VERILOG_SRCS)

obj_dir/VBeethovenTop__ALL.a: verilate
	$(MAKE) -f VBeethovenTop_hier.mk -C obj_dir USER_CPPFLAGS="$(USER_CPPFLAGS)" VM_TRACE_FST=1

	
BeethovenSim: obj_dir/VBeethovenTop__ALL.a $(SRCS) libdramsim3.so lib_beethoven.o
	c++ $(CXX_FLAGS) -o $@ $^ obj_dir/libVBeethovenTop.a obj_dir/libverilated.a obj_dir/VBeethovenTop__ALL.a -lz

endif
############ END VERILATOR ONLY ##################

############### VCS ONLY ###################
ifeq ($(SIMULATOR),vcs)

.PHONY: verilate lint
verilate: $(VERILOG_SRCS)
	verilator --cc --top BeethovenTop --trace-fst $(VERILOG_SRCS)

BeethovenSim:  $(SRCS) libdramsim3.so lib_beethoven.o libBeethovenRuntime.so
	vcs +vcs+loopreport \
		+v2k \
		-kdb \
		-timescale=1ns/100ps \
		-debug_access \
		$(VERDI_HOME)/share/PLI/VCS/LINUX64/pli.a \
		-P runtime/scripts/tab.tab \
		-sverilog \
		+incdir+$(BEETHOVEN_PATH)/build/hw \
		-full64 \
		+vpi+1 \
		+define+CLOCK_PERIOD=2 \
		+define+ICARUS=1 \
		-f $(BEETHOVEN_PATH)/build/vcs_srcs.in \
		libBeethovenRuntime.so \
		libdramsim3.so \
		lib_beethoven.o \
		-lrt \
		-L/usr/local/lib64 \
		-LDFLAGS "-Wl,-rpath=/usr/local/lib64" \
		-lbeethoven \
		-CFLAGS "-std=c++17 -I$(BEETHOVEN_PATH)/build/hw"\
		$(BEETHOVEN_PATH)/build/hw/BeethovenTopVCSHarness.v -o BeethovenTop

endif
############ END VCS ONLY ##################

.PHONY: beethoven.vvp
beethoven.vvp: $(VERILOG_SRCS)
	iverilog $(VERILOG_FLAGS) -s BeethovenTopVCSHarness -o$@ $(VERILOG_SRCS)

.PHONY: sim_icarus
sim_icarus: beethoven.vvp sim_BeethovenRuntime.vpi
	export LD_LIBRARY_PATH=$(LIB_EXPORT):$(LD_LIBRARY_PATH);\
		vvp -M. -msim_BeethovenRuntime beethoven.vvp
		#lldb -- vvp -M. -msim_BeethovenRuntime beethoven.vvp

.PHONY: clean
clean:
	rm -f beethoven.vvp \
		`find . -name '*.o'` \
		`find . -name '*.so'`\
		`find . -name '*.dylib'` \
		`find . -name '*.a'`
	rm -f sim_BeethovenRuntime.vpi
