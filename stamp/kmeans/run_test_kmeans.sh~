# Inizializzo le variabili relative ai parametri passati in ingresso allo script

#!/bin/bash

maxThread=16
runPerThread=1

#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo kmeans >> results_analysis.txt
inthread=2
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./kmeans -m10 -n10 -t0.05 -i inputs/random-n65536-d32-c16.txt -p$nthread 			
		nice -20 ./kmeans -m10 -n10 -t0.05 -i inputs/random-n65536-d32-c16.txt -p$nthread >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo kmeans >> results_analysis.txt
nthread=2
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./kmeans -m10 -n10 -t0.00005 -i inputs/random-n65536-d32-c16.txt -p$nthread                       
                nice -20 ./kmeans -m10 -n10 -t0.00005 -i inputs/random-n65536-d32-c16.txt -p$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo kmeans >> results_analysis.txt
nthread=2
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./kmeans -m5 -n5 -t0.00005 -i inputs/random-n65536-d32-c16.txt -p$nthread                       
                nice -20 ./kmeans -m5 -n5 -t0.00005 -i inputs/random-n65536-d32-c16.txt -p$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo kmeans >> results_analysis.txt
nthread=2
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./kmeans -m40 -n40 -t0.00005 -i inputs/random-n65536-d32-c16.txt -p$nthread                       
                nice -20 ./kmeans -m40 -n40 -t0.00005 -i inputs/random-n65536-d32-c16.txt -p$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done

