# Inizializzo le variabili relative ai parametri passati in ingresso allo script

#!/bin/bash

maxThread=16
runPerThread=4

#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo kmeans >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./kmeans -m10 -n10 -t0.05 -i inputs/random-n65536-d32-c16.txt -p$nthread 			
		printf "1 " >> results_analysis.txt
		nice -20 ./kmeans -m10 -n10 -t0.05 -i inputs/random-n65536-d32-c16.txt -p$nthread >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo kmeans >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./kmeans -m10 -n10 -t0.00005 -i inputs/random-n65536-d32-c16.txt -p$nthread                       
                printf "2 " >> results_analysis.txt
		nice -20 ./kmeans -m10 -n10 -t0.00005 -i inputs/random-n65536-d32-c16.txt -p$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo kmeans >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./kmeans -m5 -n5 -t0.00005 -i inputs/random-n65536-d32-c16.txt -p$nthread                       
                printf "3 " >> results_analysis.txt
		nice -20 ./kmeans -m5 -n5 -t0.00005 -i inputs/random-n65536-d32-c16.txt -p$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo kmeans >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./kmeans -m40 -n40 -t0.00005 -i inputs/random-n65536-d32-c16.txt -p$nthread                       
                printf "4 " >> results_analysis.txt
		nice -20 ./kmeans -m40 -n40 -t0.00005 -i inputs/random-n65536-d32-c16.txt -p$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done

