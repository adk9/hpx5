--------------------------------------------------------------------------------
General Instructions for building HPX-5 with APEX measurement/adaptation support
--------------------------------------------------------------------------------

To include APEX support in HPX-5, add the APEX pkgconfig path to the 
PKG_CONFIG_PATH environment variable before running ./configure. 
For example:

export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/path/to/apex/installation/lib/pkgconfig

Then, add the "--with-apex" option to the configure. For example:

./configure --enable-photon --with-mpi --with-apex

For instructions on downloading and building APEX, please see:
https://github.com/khuck/xpress-apex

To build HPX-5 with APEX on Edison, do the following:

-----------------------------------------------------
1) load the right modules
-----------------------------------------------------

module swap PrgEnv-intel PrgEnv-gnu
module load gcc/4.9.2
module load cmake/2.8.12.2
module load boost
module load automake/1.14
module load autoconf
module unload cray-shmem
module unload darshan
module load papi

-----------------------------------------------------
2) build APEX
-----------------------------------------------------

wget https://github.com/khuck/xpress-apex/archive/v0.1-beta.tar.gz
tar -xvzf v0.1-beta.tar.gz
cd xpress-apex-v0.1-beta
# run the bootstrap script
./bootstrap-configs/bootstrap-edison.sh -c -j

-----------------------------------------------------
3) Build HPX
-----------------------------------------------------

# get HPX from gitlab
git checkout git@gitlab.crest.iu.edu:extreme/hpx.git

# set environment variables for HPX for photon support
export CRAYPE_LINK_TYPE=dynamic
export HPX_PHOTON_CARGS=--with-ugni
export HPX_PHOTON_BACKEND=ugni

# set Pkgconfig for APEX
XPRESS_APEX=${HOME}/src/xpress-apex/install/lib/pkgconfig
if [ ${PKG_CONFIG_PATH} ] ; then
    export PKG_CONFIG_PATH=$XPRESS_APEX:${PKG_CONFIG_PATH}
else
    export PKG_CONFIG_PATH=$XPRESS_APEX
fi

# set the path to hpx-autotools, wherever they are
export PATH=/path/to/install/bin:$PATH
# bootstrap to make the configure script with APEX options
./bootstrap
# run configure
./configure --prefix=--prefix=/path/to/install --enable-mpi CC=cc HPX_PHOTON_CARGS=--with-ugni --enable-photon --with-apex --enable-debug
make && make install

-----------------------------------------------------
4) Build LULESH
-----------------------------------------------------

# (get hpx-apps from gitlab)
cd hpx-apps/lulesh/parcels

# edit the Makefile to include pkg-config info for APEX. Here's the git diff:

====================
--- a/lulesh/parcels/Makefile
+++ b/lulesh/parcels/Makefile
@@ -1,8 +1,11 @@
+
 CC = gcc 
 CFLAGS = -g -std=gnu99 -Wno-unused -Wno-implicit-function-declaration -Wno-unused-variable -Wno-uninitialized
 
 LIBS    = $(shell pkg-config --libs hpx)
+LIBS   += $(shell pkg-config --libs apex)
 CFLAGS += $(shell pkg-config --cflags hpx)
+CFLAGS += $(shell pkg-config --cflags apex) -DHAVE_APEX
 
 LINKER = $(CC)
 #LINKER  = libtool --mode=link $(CC)
====================

# edit lulesh-hpx.c to include apex.h and initialize throttling. Here's the git diff:

====================
--- a/lulesh/parcels/lulesh-hpx.c
+++ b/lulesh/parcels/lulesh-hpx.c
@@ -1,6 +1,10 @@
 #include "lulesh-hpx.h"
 #include <hpx/lco.h>
 
+#if defined(HAVE_APEX)
+#include "apex.h"
+#endif
+
 static hpx_action_t _main          = 0;
 static hpx_action_t _advanceDomain = 0;
 static hpx_action_t _initDomain    = 0;
@@ -410,6 +414,10 @@ int main(int argc, char **argv)
   HPX_REGISTER_ACTION(_MonoQ_sends_action, &_MonoQ_sends);
   HPX_REGISTER_ACTION(_MonoQ_result_action, &_MonoQ_result);
 
+#ifdef HAVE_APEX
+  apex_setup_power_cap_throttling();
+#endif
+
   int input[3];
   input[0] = nDoms;
   input[1] = nx;
====================

make

# Set runtime environment variables for APEX before the aprun call.

export APEX_PROFILE_OUTPUT=0
export APEX_SCREEN_OUTPUT=1
export APEX_POLICY=1
export APEX_CONCURRENCY=1
export APEX_THROTTLING=1
export APEX_ENERGY_THROTTLING=1
export APEX_THROTTLING_MAX_THREADS=48
export APEX_THROTTLING_MIN_THREADS=12
export APEX_THROTTLING_MAX_WATTS=200.0
export APEX_THROTTLING_MIN_WATTS=180.0

aprun -n 16 -N 1 -j2 -d 48 ./luleshparcels -n 343 -x 48 -i 100 --hpx-threads=48

# To see the concurrency charts (contact khuck@cs.uoregon.edu for a python script
# to aggregate them into one script):

module load gnuplot
gnuplot -persist concurrency.*.gnuplot