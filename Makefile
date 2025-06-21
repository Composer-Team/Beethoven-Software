PLATFORM = discrete
TARGET = sim
DEVICE = kria
SUDO ?= sudo
PREFIX ?= /usr/local/
BUILD_TYPE ?= DEBUG

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

# DEBUG FLAGS
CXX_FLAGS += -O0 -g3 
# RELEASE FLAGS
#CXX_FLAGS += -O2
FRONTBUS=axi
SIMULATOR=vpi
VPI_LOC = /usr/local/lib/ivl
VPI_FLAGS = $(VPI_LOC)/system.vpi
VERILOG_FLAGS = -DCLOCK_PERIOD=500 -DICARUS
VERILOG_SRCS = $(shell cat ${BEETHOVEN_PATH}/build/vcs_srcs.in) ${BEETHOVEN_PATH}/build/hw/BeethovenTopVCSHarness.v

# vpi
CXX_FLAGS += -DSIM=vcs

libdramsim3.so:
	$(MAKE) -C runtime/DRAMsim3/ -j
	cp $(DRAMSIM3LIB) .

SRCS = 	runtime/src/data_server.o \
	runtime/src/cmd_server.o \
	runtime/src/mmio.o \
	runtime/src/sim/axi/front_bus_ctrl_axi.o \
	runtime/src/sim/axi/${SIMULATOR}_axi_frontend.o  \
	runtime/src/sim/tick.o \
	runtime/src/sim/mem_ctrl.o \
	src/verilator_server.o \
	src/rocc_response.o \
	src/util.o \
	src/response_handle.o \
	src/allocator/alloc.o \
	src/rocc_cmd.o \
	$(HANDLE_O)

lib_beethoven.o: ${BEETHOVEN_PATH}/build/beethoven_hardware.cc ${BEETHOVEN_PATH}/build/beethoven_hardware.h
	$(CXX) -c $(CXX_FLAGS) -o$@ ${BEETHOVEN_PATH}/build/beethoven_hardware.cc

runtime/src/%.o: runtime/src/%.cc
	$(CXX) -c ${CXX_FLAGS} -o$@ $^

src/%.o: src/%.cc
	$(CXX) -c ${CXX_FLAGS} $(SW_DEF) -o$@ $^

sim_BeethovenRuntime.vpi: $(SRCS) lib_beethoven.o libdramsim3.so
	$(CXX) -shared $(LD_FLAGS) -o$@ $^

TESTS = bin/alloc_sizes bin/merge_sort

bin/%: test/%.cc $(SRCS) libdramsim3.so
	mkdir -p bin
	$(CXX) -c ${CXX_FLAGS} $< -o $(basename $<)
	$(CXX) $(LD_FLAGS) -o $@ $(SRCS) $(basename $<)

test: $(TESTS)

.PHONY: beethoven.vvp
beethoven.vvp:
	iverilog $(VERILOG_FLAGS) -s BeethovenTopVCSHarness -o$@ $(VERILOG_SRCS)

.PHONY: sim_icarus
sim_icarus: sim_BeethovenRuntime.vpi beethoven.vvp
	export LD_LIBRARY_PATH=$(LIB_EXPORT):$(LD_LIBRARY_PATH);\
		vvp -M. -msim_BeethovenRuntime beethoven.vvp
		#lldb -- vvp -M. -msim_BeethovenRuntime beethoven.vvp

.PHONY: clean
clean:
	rm -f beethoven.vvp \
		`find . -name '*.o'` \
		`find . -name '*.so'`\
	       	`find . -name '*.dylib'`
