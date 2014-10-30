# Makefile
########
#ARCH = `uname -s` . `uname -m`
#ARCH = Linux.i686
#SUNDIALS_DIR = /home/astro/sundials

########
# Stuff that depends on the specific program we're compiling
# To use this file for other programs, you'll need to change things
# in this section.
########

# The name of the output program 
PROG = depicx

# Object files that go into the final executable
OBJS = depicx.o mpi_system.o reduce.o allreduce.o bcast.o scatterv.o gather.o gatherv.o alltoall.o allgather.o alltoallv.o p2p.o logging.o luleshMPI.o 
#OBJS = depicx.o mpi_system.o reduce.o allreduce.o bcast.o scatterv.o gather.o gatherv.o alltoall.o allgather.o alltoallv.o p2p.o luleshMPI_OMP.o

# Header files 
HDRS = hpx_mpif.h mpi_wrapper.h mpi_system.h

########
# Some stuff that's the same for all programs and machines 
########

# The commands we use
CC             := gcc
CXX            := g++
#CC             := gcc vtcc -vt:hyb
#CXX            := g++ vtCC -vt:hyb 
#CC             := vtcc -vt:inst manual -DVTRACE
#CXX            := vtCC -vt:inst manual -DVTRACE
# F90          := dragonegg-gfortran
# FC           := dragonegg-gfortran
F90            := gfortran
FC             := gfortran
OPT            := opt
LLC            := llc

# A list of flag sets that we will combine
#FLAGSETS       := hpx epicx 3t
FLAGSETS       := hpx epicx

# Force some flags for the EpicX code
CPPFLAGS.epicx :=
CXXFLAGS.epicx := -g -O3 -mcmodel=medium -pedantic -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -fpermissive #-pg #-fprofile-arcs -ftest-coverage
#CXXFLAGS.epicx := -mcmodel=medium -pedantic -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -fpermissive
#CFLAGS.epicx   := -g -std=gnu99 -mcmodel=medium -pedantic -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable
CFLAGS.epicx   := -g -O3 -std=gnu99 -mcmodel=medium -pedantic -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable #-DLOGGING_GENERIC -DLOGGING_P2P #-DLOGGING_LOCKS #-pg #-fprofile-arcs -ftest-coverage
FFLAGS.epicx   :=
F90FLAGS.epicx := 
LDFLAGS.epicx  := -g -O3 -static-libgfortran #-pg
#LDFLAGS.epicx  := -static-libgfortran
LIBS.epicx     := #-lgcov

# Grab HPX stuff using pkg-config (requires module add hpx)
#CPPFLAGS.hpx   := $(shell pkg-config --cflags-only-I libhpx)
#CFLAGS.hpx     := $(shell pkg-config --cflags-only-other libhpx)
#CXXFLAGS.hpx   := $(shell pkg-config --cflags-only-other libhpx)
#LDFLAGS.hpx    := $(shell pkg-config --libs-only-L libhpx)
#LIBS.hpx       := $(shell pkg-config --libs-only-l libhpx)
CPPFLAGS.hpx   := -I$(HOME)/hpx5_gitlab/include  
CFLAGS.hpx     := -I$(HOME)/hpx5_gitlab/include  
CXXFLAGS.hpx   := -I$(HOME)/hpx5_gitlab/include  
LDFLAGS.hpx    := -L/opt/openmpi/intel/1.7.5/lib/ -L$(HOME)/hpx5_gitlab/lib
LIBS.hpx       := -lhpx -lmpi #-lhpx 

# Set up some flags for the 3T transformation
THREADS.3t     := 32
OPTFLAGS.3t    := -load lib3t.so -g2tl -g2tl-max-threads $(THREADS.3t)
FFLAGS.3t      := -fplugin-arg-dragonegg-emit-ir

# Combine flag sets with command-line flags
_combine        = $(strip $(foreach set,$(FLAGSETS),$(strip $($(1).$(set)))) $($(1)))
CPPFLAGS.all    = $(call _combine,CPPFLAGS)
CXXFLAGS.all    = $(call _combine,CXXFLAGS)
CFLAGS.all      = $(call _combine,CFLAGS)
FFLAGS.all      = $(call _combine,FFLAGS)
F90FLAGS.all    = $(call _combine,F90FLAGS)
OPTFLAGS.all    = $(call _combine,OPTFLAGS)
LDFLAGS.all     = $(call _combine,LDFLAGS)
LIBS.all        = $(call _combine,LIBS)

# Composite commands used in rules below
INSTRUMENT      = $(OPT) $(OPTFLAGS.all)
ASSEMBLE        = $(LLC) -filetype=obj
#COMPILE.f90     = $(F90) $(F90FLAGS.all) $(FFLAGS.all) -S -o - $< | $(INSTRUMENT) -o - | $(ASSEMBLE) -o $@
COMPILE.f90     = $(F90) $(F90FLAGS.all) -c -o $@ $< 
COMPILE.c       = $(CC) $(CPPFLAGS.all) $(CFLAGS.all) -c -o $@ $<
COMPILE.cxx     = $(CXX) $(CPPFLAGS.all) $(CXXFLAGS.all) -c -o $@ $<
LINK            = $(CXX) $(LDFLAGS.all) -o $@ $^ $(LIBS.all)

#LDLIBS = -lida.$(ARCH) -lshared.$(ARCH) -lnvecserial.$(ARCH) -lm
#LDLIBS = -lcvodes.$(ARCH) -lshared.$(ARCH) -lnvecserial.$(ARCH) -lm -lhdf5

########
# Finally, the commands that actually make stuff
# You shouldn't have to change anything here 
# These commands are of the form:
#   output_thing:  stuff it depends on
#   <TAB> command to make output_thing 
########

.PHONY: all clean dataclean

all: $(PROG)

# re-link the program when the object files change
$(PROG): $(OBJS)
	$(LINK)

clean:
	$(RM) $(PROG) $(OBJS) $(OBJS:.o=.d) *.sdf *.matlab *.idl core*

dataclean:
	$(RM) $(OBJS) *.sdf *.matlab *.idl core*

%.o: %.c
	$(COMPILE.c)

%.o: %.cc
	$(COMPILE.cxx)

%.o: %.f90
	$(COMPILE.f90)

%.d: %.c
	@set -e; rm -f $@; \
	 $(CC) -M $(CPPFLAGS.all) $< > $@.$$$$; \
	 sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	 rm -f $@.$$$$

%.d: %.f90
	@set -e; rm -f $@; \
	 $(F90) -M -cpp $(CPPFLAGS.all) $< > $@.$$$$; \
	 sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	 rm -f $@.$$$$

%.d: %.f
	@set -e; rm -f $@; \
	 $(FC) -M -cpp $(CPPFLAGS.all) $< > $@.$$$$; \
	 sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	 rm -f $@.$$$$

-include $(OBJS:.o=.d)
