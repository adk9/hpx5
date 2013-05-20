HARDWARE=verbs

# OpenMPI 1.2.5
# export MPICC=/usr/local/bin/mpicc

# OpenMPI 1.3a1
#export MPICC=/usr/local/openmpi-1.3a1/bin/mpicc

# MVAPICH
#export MPICC=/usr/local/mvapich/bin/mpicc

# MVAPICH-uDAPL
#export MPICC=/usr/local/mvapich-udapl/bin/mpicc

# MPICH2
#export MPICC=/work/kissel/mpich2-1.3.1/bin/mpicc

# OpenMPI 1.4
#export MPICC=/usr/openmpi1.4/bin/mpicc

export MPICC=mpicc

PREFIX := src

ifeq ($(HARDWARE),dapl_overmx)
SUBDIRS := \
	dapl_overmx_util \
	dapl_overmx
endif

ifeq ($(HARDWARE),dapl)
SUBDIRS := \
	dapl_util \
	dapl
endif

ifeq ($(HARDWARE),verbs)
SUBDIRS := \
	verbs_util \
	verbs
endif

all:
	+@for d in $(SUBDIRS); do \
		echo $(MAKE) -C $(PREFIX)/$$d; \
		$(MAKE) -C $(PREFIX)/$$d; \
		retval=$$?; \
		if [ $$retval != 0 ]; then \
				exit $$retval; \
		fi; \
	done;

debug:
	$(MAKE) all DEBUG=yes

xsp:
	$(MAKE) debug XSP=yes

xsp_mt:
	$(MAKE) debug XSP=yes MULTITHREADED=yes

clean:
	+@for d in $(SUBDIRS); do \
		echo $(MAKE) -C $(PREFIX)/$$d clean; \
		$(MAKE) -C $(PREFIX)/$$d clean; \
		retval=$$?; \
		if [ $$retval != 0 ]; then \
				exit $$retval; \
		fi; \
	done;
