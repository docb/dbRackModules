# If RACK_DIR is not defined when calling the Makefile, default to two directories above
RACK_DIR ?= ../..

# FLAGS will be passed to both the C and C++ compiler
FLAGS += -I Gamma
CFLAGS +=
CXXFLAGS +=

# Careful about linking to shared libraries, since you can't assume much about the user's environment and library search path.
# Static libraries are fine, but they should be added to this plugin's build system.
LDFLAGS +=

# Add .cpp files to the build
SOURCES += $(wildcard src/*.cpp)
SOURCES += Gamma/src/arr.cpp
SOURCES += Gamma/src/Domain.cpp
SOURCES += Gamma/src/scl.cpp
SOURCES += Gamma/src/DFT.cpp
SOURCES += Gamma/src/FFT_fftpack.cpp
SOURCES += Gamma/src/fftpack++1.cpp
SOURCES += Gamma/src/fftpack++2.cpp


# Add files to the ZIP package when running `make dist`
# The compiled plugin and "plugin.json" are automatically added.
DISTRIBUTABLES += res
DISTRIBUTABLES += presets
DISTRIBUTABLES += $(wildcard LICENSE*)

BINARIES += src/data/bd/bd01.raw
BINARIES += src/data/bd/bd02.raw
BINARIES += src/data/bd/bd03.raw
BINARIES += src/data/bd/bd04.raw
BINARIES += src/data/bd/bd05.raw
BINARIES += src/data/bd/bd06.raw
BINARIES += src/data/bd/bd07.raw
BINARIES += src/data/bd/bd08.raw
BINARIES += src/data/bd/bd09.raw
BINARIES += src/data/bd/bd10.raw
BINARIES += src/data/bd/bd11.raw
BINARIES += src/data/bd/bd12.raw
BINARIES += src/data/bd/bd13.raw
BINARIES += src/data/bd/bd14.raw
BINARIES += src/data/bd/bd15.raw
BINARIES += src/data/bd/bd16.raw
BINARIES += src/data/sn/sn01.raw
BINARIES += src/data/sn/sn02.raw
BINARIES += src/data/sn/sn03.raw
BINARIES += src/data/sn/sn04.raw
BINARIES += src/data/sn/sn05.raw
BINARIES += src/data/sn/sn06.raw
BINARIES += src/data/sn/sn07.raw
BINARIES += src/data/sn/sn08.raw
BINARIES += src/data/sn/sn09.raw
BINARIES += src/data/sn/sn10.raw
BINARIES += src/data/sn/sn11.raw
BINARIES += src/data/sn/sn12.raw
BINARIES += src/data/sn/sn13.raw
BINARIES += src/data/sn/sn14.raw
BINARIES += src/data/sn/sn15.raw
BINARIES += src/data/sn/sn16.raw
BINARIES += src/data/cl/cl01.raw
BINARIES += src/data/cl/cl02.raw
BINARIES += src/data/cl/cl03.raw
BINARIES += src/data/cl/cl04.raw
BINARIES += src/data/cl/cl05.raw
BINARIES += src/data/cl/cl06.raw
BINARIES += src/data/cl/cl07.raw
BINARIES += src/data/cl/cl08.raw
BINARIES += src/data/cl/cl09.raw
BINARIES += src/data/cl/cl10.raw
BINARIES += src/data/cl/cl11.raw
BINARIES += src/data/cl/cl12.raw
BINARIES += src/data/cl/cl13.raw
BINARIES += src/data/cl/cl14.raw
BINARIES += src/data/cl/cl15.raw
BINARIES += src/data/cl/cl16.raw
BINARIES += src/data/oh/oh01.raw
BINARIES += src/data/oh/oh02.raw
BINARIES += src/data/oh/oh03.raw
BINARIES += src/data/oh/oh04.raw
BINARIES += src/data/oh/oh05.raw
BINARIES += src/data/oh/oh06.raw
BINARIES += src/data/oh/oh07.raw
BINARIES += src/data/oh/oh08.raw
BINARIES += src/data/oh/oh09.raw
BINARIES += src/data/oh/oh10.raw
BINARIES += src/data/oh/oh11.raw
BINARIES += src/data/oh/oh12.raw
BINARIES += src/data/oh/oh13.raw
BINARIES += src/data/oh/oh14.raw
BINARIES += src/data/oh/oh15.raw
BINARIES += src/data/oh/oh16.raw
BINARIES += src/data/perc/perc01.raw
BINARIES += src/data/perc/perc02.raw
BINARIES += src/data/perc/perc03.raw
BINARIES += src/data/perc/perc04.raw
BINARIES += src/data/perc/perc05.raw
BINARIES += src/data/perc/perc06.raw
BINARIES += src/data/perc/perc07.raw
BINARIES += src/data/perc/perc08.raw
BINARIES += src/data/perc/perc09.raw
BINARIES += src/data/perc/perc10.raw
BINARIES += src/data/perc/perc11.raw
BINARIES += src/data/perc/perc12.raw
BINARIES += src/data/perc/perc13.raw
BINARIES += src/data/perc/perc14.raw
BINARIES += src/data/perc/perc15.raw
BINARIES += src/data/perc/perc16.raw
BINARIES += src/data/clap/clap01.raw
BINARIES += src/data/clap/clap02.raw
BINARIES += src/data/clap/clap03.raw
BINARIES += src/data/clap/clap04.raw
BINARIES += src/data/clap/clap05.raw
BINARIES += src/data/clap/clap06.raw
BINARIES += src/data/clap/clap07.raw
BINARIES += src/data/clap/clap08.raw
BINARIES += src/data/clap/clap09.raw
BINARIES += src/data/clap/clap10.raw
BINARIES += src/data/clap/clap11.raw
BINARIES += src/data/clap/clap12.raw
BINARIES += src/data/clap/clap13.raw
BINARIES += src/data/clap/clap14.raw
BINARIES += src/data/clap/clap15.raw
BINARIES += src/data/clap/clap16.raw


# Include the Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk
