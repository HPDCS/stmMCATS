# Inizializzo le variabili relative ai parametri passati in ingresso allo script

#!/bin/bash

maxThread=16
runPerThread=1

#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo vacation >> results_analysis.txt
inthread=2
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./vacation -n2 -q60 -u90 -r32768 -t1048576 -t$nthread 			
		nice -20 ./vacation -n2 -q60 -u90 -r32768 -t1048576 -t$nthread >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo vacation >> results_analysis.txt
nthread=2
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./vacation -n2 -q90 -u98 -r1048576 -t2097152 -t$nthread                       
                nice -20 ./vacation -n2 -q90 -u98 -r1048576 -t2097152 -t$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo vacation >> results_analysis.txt
nthread=2
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./vacation -n4 -q60 -u90 -r32768 -t524288 -t$nthread                       
                nice -20 ./vacation -n4 -q60 -u90 -r32768 -t524288 -t$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done

