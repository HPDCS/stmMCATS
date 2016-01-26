# Inizializzo le variabili relative ai parametri passati in ingresso allo script

#!/bin/bash

maxThread=16
runPerThread=1

#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo yada >> results_analysis.txt
inthread=2
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./yada -a15 -i inputs/ttimeu100000.2 -c$nthread 			
		nice -20 ./yada -a15 -i inputs/ttimeu100000.2 -c$nthread >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo yada >> results_analysis.txt
nthread=2
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./yada -a10 -i inputs/ttimeu1000000.2 -c$nthread                       
                nice -20 ../yada -a10 -i inputs/ttimeu1000000.2 -c$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo yada >> results_analysis.txt
nthread=2
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./yada -a20 -i inputs/ttimeu100000.2 -c$nthread                       
                nice -20 ./yada -a20 -i inputs/ttimeu100000.2 -c$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done

