#!/bin/bash

cd tinySTM
make clean
make
cd ..
cd stamp/intruder
make -f Makefile.stm clean
make -f Makefile.stm
cd ..
cd yada
make -f Makefile.stm clean
make -f Makefile.stm
cd ..
cd vacation
make -f Makefile.stm clean
make -f Makefile.stm


