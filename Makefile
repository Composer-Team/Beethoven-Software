PLATFORM = discrete
TARGET = sim
DEVICE = kria
SIMULATOR ?= icarus
RUNTIME_DIR ?= $(shell pwd)/runtime

OBJDIR = obj
ROBJDIR = robj

SRC_PREFIX ?=

SUDO ?= sudo
PREFIX ?= /usr/local/
BUILD_MODE ?= DEBUG

STATIC_LIB ?= 1

VPATH=$(BEETHOVEN_PATH)/build/hw/verification

VERILATOR_INC = $(shell verilator --getenv VERILATOR_ROOT)/include/

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
	    -O0 -g3 \
	    -I${BEETHOVEN_PATH}/build/ \
	    -I/usr/local/include/iverilog/ \
	    -Iinclude/ \
	    -I/usr/local/include/beethoven \
	    -IDRAMsim3/src/ \
	    -IDRAMsim3/ext/headers/ \
	    -Iinclude -I$(RUNTIME_DIR)/include -I$(RUNTIME_DIR)/DRAMsim3/src \
	    -I$(RUNTIME_DIR)/DRAMsim3/ext/headers -I$(BEETHOVEN_PATH)/build/

ifeq ($(BUILD_MODE),Debug)
CXX_FLAGS += -O0 -g3 
VCS_FLAGS =
endif

ifeq ($(BUILD_MODE),Release)
CXX_FLAGS += -O3
VCS_FLAGS = +rad -CFLAGS "-O3"
endif
PWD = $(shell pwd)

UNAME_S:=$(shell uname -s)
LD_FLAGS = -L/usr/local/lib/ivl/ -ldramsim3 -lbeethoven

ifeq ($(UNAME_S),Linux)
	LIB_EXPORT = export LD_LIBRARY_PATH=/usr/local/lib64:$(RUNTIME_DIR)/DRAMsim3/
endif

ifeq ($(UNAME_S),Darwin)
	LD_FLAGS += -rpath /usr/local/lib -undefined suppress
	LIB_EXPORT="export LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):$(RUNTIME_DIR)/DRAMsim3/"
endif

VPI_LOC = /usr/local/lib/ivl
VPI_FLAGS = $(VPI_LOC)/system.vpi
VERILOG_FLAGS = -DCLOCK_PERIOD=4 -I${BEETHOVEN_PATH}/build/hw -DSIM
VERILOG_SRCS = $(shell cat ${BEETHOVEN_PATH}/build/vcs_srcs.in) 


FRONTBUS=axi

SRCS = 	$(ROBJDIR)/data_server.o \
	$(ROBJDIR)/cmd_server.o \
	$(ROBJDIR)/mmio.o \
	$(ROBJDIR)/front_bus_ctrl_axi.o \
	$(ROBJDIR)/tick.o \
	$(ROBJDIR)/mem_ctrl.o \
	$(ROBJDIR)/DataWrapper.o \
	$(HANDLE_O)

ifeq ($(STATIC_LIB),1)
SRCS += $(OBJDIR)/verilator_server.o \
	$(OBJDIR)/rocc_response.o \
	$(OBJDIR)/util.o \
	$(OBJDIR)/response_handle.o \
	$(OBJDIR)/allocator/alloc.o \
	$(OBJDIR)/rocc_cmd.o
endif

ifeq ($(SIMULATOR),icarus)
SIMULATOR_BACKEND=vpi
DEPS = sim_BeethovenRuntime.vpi beethoven.vvp
LD_FLAGS += -lvpi

VERILOG_FLAGS += -DICARUS -g2005-sv -I$(VPATH) -I$(VPATH)/assume -I$(VPATH)/assert -I$(VPATH)/cover -DSYNTHESIS
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
SRCS += $(ROBJDIR)/verilator.o
endif

SRCS += $(ROBJDIR)/${SIMULATOR_BACKEND}_axi_frontend.o

lib_beethoven.o: ${BEETHOVEN_PATH}/build/beethoven_hardware.cc ${BEETHOVEN_PATH}/build/beethoven_hardware.h
	$(CXX) -c $(CXX_FLAGS) -o$@ ${BEETHOVEN_PATH}/build/beethoven_hardware.cc

$(ROBJDIR)/%.o: $(RUNTIME_DIR)/src/%.cc $(CXX_DEPS)
	@mkdir -p $(ROBJDIR)
	$(CXX) -c ${CXX_FLAGS} -o$@ $<

$(OBJDIR)/%.o: $(SRC_PREFIX)src/%.cc
	@mkdir -p $(OBJDIR) $(OBJDIR)/allocator
	$(CXX) -c ${CXX_FLAGS} $(SW_DEF) -o$@ $^

# for icarus - requires .vpi ending
sim_BeethovenRuntime.vpi: $(SRCS) lib_beethoven.o
	$(CXX) -shared -o$@  $^ $(LD_FLAGS) -lveriuser -lvpi

# for VCS, allows standard .so ending
libBeethovenRuntime.so: $(SRCS) lib_beethoven.o
	$(CXX) -shared -o$@ $^ $(LD_FLAGS)



TESTS = bin/alloc_sizes bin/merge_sort

bin/%: test/%.cc $(SRCS)
	mkdir -p bin
	$(CXX) -c ${CXX_FLAGS} $< -o $(basename $<)
	$(CXX) -o $@ $(SRCS) $(basename $<) $(LD_FLAGS)

test: $(TESTS)

lint: $(VERILOG_SRCS)
	verilator --lint-only -Wno-timescalemod -Wall +incdir+$(BEETHOVEN_PATH)/build/hw -top BeethovenTop $(VERILOG_SRCS)



.PHONY: beethoven.vvp
beethoven.vvp: $(VERILOG_SRCS)
	iverilog $(VERILOG_FLAGS) -s BeethovenTopVCSHarness -o$@ $(VERILOG_SRCS)

############### VERILATOR ONLY ###################
ifeq ($(SIMULATOR),verilator)

VERILATOR_DISABLE_WARN=-Wno-ascrange -Wno-pinmissing -Wno-widthexpand -Wno-widthtrunc

.PHONY: verilate lint
verilate: $(VERILOG_SRCS)
	echo $(shell pwd)
	mkdir -p objdir
	verilator $(VERILATOR_DISABLE_WARN) --cc --top BeethovenTop --trace-fst +incdir+$(BEETHOVEN_PATH)/build/hw  +incdir+$(VPATH)/cover +incdir+$(VPATH)/assume +incdir+$(VPATH)/assert +incdir+$(VPATH) $(VERILOG_SRCS)

obj_dir/VBeethovenTop__ALL.a: verilate
	$(MAKE) -f VBeethovenTop.mk -C obj_dir USER_CPPFLAGS="$(USER_CPPFLAGS)" VM_TRACE_FST=1

	
BeethovenSim: obj_dir/VBeethovenTop__ALL.a $(SRCS) lib_beethoven.o
	c++ $(CXX_FLAGS) -o $@ $^ obj_dir/libVBeethovenTop.a obj_dir/libverilated.a obj_dir/VBeethovenTop__ALL.a -lz $(LD_FLAGS)

endif
############ END VERILATOR ONLY ##################

############### VCS ONLY ###################
ifeq ($(SIMULATOR),vcs)

.PHONY: verilate lint
verilate: $(VERILOG_SRCS)
	verilator --cc --top BeethovenTop --trace-fst $(VERILOG_SRCS)

BeethovenSim:  $(SRCS) lib_beethoven.o libBeethovenRuntime.so
	vcs +vcs+loopreport \
		-timescale=1ns/100ps \
		$(VERDI_HOME)/share/PLI/VCS/LINUX64/pli.a \
		-P $(RUNTIME_DIR)/scripts/tab.tab \
		-sverilog \
		+incdir+$(BEETHOVEN_PATH)/build/hw \
		+incdir+$(VPATH)/assume \
		+incdir+$(VPATH)/cover \
		+incdir+$(VPATH)/assert \
		+incdir+$(VPATH) \
		-full64 \
		+vpi+1 \
		+define+CLOCK_PERIOD=2 \
		+define+ICARUS=1 \
		-f $(BEETHOVEN_PATH)/build/vcs_srcs.in \
		libBeethovenRuntime.so \
		lib_beethoven.o \
		-lrt \
		-L/usr/local/lib64 \
		-LDFLAGS "-Wl,-rpath=/usr/local/lib64" \
		-lbeethoven -ldramsim3 \
		-CFLAGS "-std=c++17 -I$(BEETHOVEN_PATH)/build/hw"\
		$(VCS_FLAGS) \
		$(BEETHOVEN_PATH)/build/hw/BeethovenTopVCSHarness.v -o BeethovenTop

endif
############ END VCS ONLY ##################

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
	rm -rf obj_dir
	rm -f sim_BeethovenRuntime.vpi
