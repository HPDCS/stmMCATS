#!/bin/bash

cd tinySTM
make clean
make
cd ..
cd Stamp/intruder
make -f Makefile.stm clean
make -f Makefile.stm
cd ..
cd genome
make -f Makefile.stm clean
make -f Makefile.stm
cd ..
cd vacation
make -f Makefile.stm clean
make -f Makefile.stm
cd ..
cd kmeans
make -f Makefile.stm clean
make -f Makefile.stm

echo "Compilazione terminata"

