#!/bin/bash 

rm -rf run_spack.out 
rm -rf spack_albany.out 
rm -rf spack 
rm -rf spack_ctest.out 
rm -rf results_spack 
rm -rf /tmp/ikalash/spack*
 
echo "Cloning spack repo..."
git clone git@github.com:SNLComputation/spack.git
cd spack
git checkout albany
echo "...done."
export http_proxy=http://wwwproxy.sandia.gov:80
export https_proxy=http://wwwproxy.sandia.gov:80
. share/spack/setup-env.sh 
echo "Starting spack build..."
spack --insecure install --dirty --keep-stage albany >& spack_albany.out 
cd /home/ikalash/Trilinos_Albany/nightlyAlbanyTests
mv spack/spack_albany.out .
echo "...done."
cd spack
spack cd albany
cd ../spack-build
echo "Starting running of tests..."
PWD=`pwd`
echo "  spack build dir = " $PWD
ctest -V >& spack_ctest.out 
mv spack_ctest.out /home/ikalash/Trilinos_Albany/nightlyAlbanyTests/spack_ctest.out 
echo "...done."
cd /home/ikalash/Trilinos_Albany/nightlyAlbanyTests 
bash process_results_spack.sh 
bash send_email_spack.sh 