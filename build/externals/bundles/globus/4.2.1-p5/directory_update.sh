#!/bin/sh

cd source-trees/xio/src/builtins 
mkdir -p gsi popen 
sed -e 's/globus_xio_gsi.h/globus_xio_gsi_driver.h/' ../../drivers/gsi/globus_i_xio_gsi.h > gsi/globus_i_xio_gsi.h 
cp ../../drivers/gsi/globus_xio_gsi.h gsi/globus_xio_gsi_driver.c 
cp ../../drivers/gsi/globus_xio_gsi.c gsi/globus_xio_gsi_driver.h 
cp ../../drivers/popen/source/*.[hc] popen 
sed -e 's/tcp/gsi/g' tcp/Makefile.am > gsi/Makefile.am 
sed -e 's/tcp/gsi/g' tcp/Makefile.in > gsi/Makefile.in 
sed -e 's/tcp/popen/g' tcp/Makefile.am > popen/Makefile.am
sed -e 's/tcp/popen/g' tcp/Makefile.in > popen/Makefile.in
# Note: The below doesn't make *any sense given the above + it causes errors
#cd gsi 
#ln -s ../../../../../source-trees/xio/src/builtins/gsi/Makefile.am . 
#ln -s ../../../../../source-trees/xio/src/builtins/gsi/Makefile.in . 
#ln -s ../../../../../source-trees/xio/src/builtins/gsi/globus_i_xio_gsi.h . 
#ln -s ../../../../../source-trees/xio/src/builtins/gsi/globus_xio_gsi_driver.c . 
#ln -s ../../../../../source-trees/xio/src/builtins/gsi/globus_xio_gsi_driver.h . 
#cd ../popen 
#ln -s ../../../../../source-trees/xio/src/builtins/popen/Makefile.am . 
#ln -s ../../../../../source-trees/xio/src/builtins/popen/Makefile.in . 
#ln -s ../../../../../source-trees/xio/src/builtins/popen/globus_xio_popen_driver.c . 
#ln -s ../../../../../source-trees/xio/src/builtins/popen/globus_xio_popen_driver.h . 
cd ../../../../
