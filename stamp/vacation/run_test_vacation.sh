# Inizializzo le variabili relative ai parametri passati in ingresso allo script

#!/bin/bash

maxThread=16
runPerThread=4

#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo vacation >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./vacation -n2 -q60 -u90 -r32768 -t1048576 -c$nthread 			
		printf "1 " >> results_analysis.txt
		nice -20 ./vacation -n2 -q60 -u90 -r32768 -t1048576 -c$nthread >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo vacation >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./vacation -n2 -q90 -u98 -r1048576 -t2097152 -c$nthread                       
                printf "2 " >> results_analysis.txt
		nice -20 ./vacation -n2 -q90 -u98 -r1048576 -t2097152 -c$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo vacation >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./vacation -n4 -q60 -u90 -r32768 -t524288 -c$nthread                       
                printf "3 " >> results_analysis.txt
		nice -20 ./vacation -n4 -q60 -u90 -r32768 -t524288 -c$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done

#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo vacation >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./vacation -n4 -q60 -u98 -r1048576 -t4194304 -c$nthread                       
                printf "4 " >> results_analysis.txt
		nice -20 ./vacation -n4 -q60 -u98 -r1048576 -t4194304 -c$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done
