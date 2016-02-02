# Inizializzo le variabili relative ai parametri passati in ingresso allo script

#!/bin/bash

maxThread=16
runPerThread=1

#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo bayes >> results_analysis.txt
nthread=2
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./bayes -v32 -r2048 -n10 -p40 -i2 -e8 -s1 -t$nthread 			
		nice -20 ./bayes -v32 -r8096 -n10 -p30 -i2 -e10 -s1 -t$nthread >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo bayes >> results_analysis.txt
nthread=2
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./bayes -v32 -r4096 -n10 -p40 -i2 -e8 -s1 -t$nthread                       
                nice -20 ./bayes -v32 -r8096 -n10 -p30 -i2 -e10 -s1 -t$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo bayes >> results_analysis.txt
nthread=2
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./bayes -v32 -r8096 -n10 -p30 -i2 -e10 -s1 -t$nthread                       
                nice -20 ./bayes -v32 -r8096 -n10 -p30 -i2 -e10 -s1 -t$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo bayes >> results_analysis.txt
nthread=2
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./bayes -v12 -r16384 -n20 -p20 -i2 -e20 -s1 -t$nthread                       
                nice -20 ./bayes -v32 -r8096 -n10 -p30 -i2 -e10 -s1 -t$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done
