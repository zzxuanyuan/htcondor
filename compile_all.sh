#!/bin/sh

make
sudo make install
condor_configure --install=/usr/local --install-dir=$(pwd)/release_dir --central-manager=condormaster.cluster.unl.edu --overwrite
echo "LD_LIBRARY_PATH=\"/usr/local/lib:/usr/lib64:/home/centos/htcondor/release_dir/lib:\$LD_LIBRARY_PATH\"
export LD_LIBRARY_PATH" >> ~/htcondor/release_dir/condor.sh
