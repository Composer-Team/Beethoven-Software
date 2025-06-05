cmake .. -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_INSTALL_PREFIX=$HOME/INSTALL  -DCMAKE_EXE_LINKER_FLAGS="-stdlib=libc++ -L$HOME/INSTALL/lib -lc++ -lc++abi -lLTO" -DCMAKE_CXX_FLAGS="-I/home/chris/INSTALL/include/c++/v1" -DCMAKE_BUILD_TYPE=Debug -DDRAMSIM_CONFIG=custom_dram_configs/hyperram.ini
make -j8
bash ../build_vcs.sh
