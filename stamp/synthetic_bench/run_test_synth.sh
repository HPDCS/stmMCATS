# Inizializzo le variabili relative ai parametri passati in ingresso allo script

#!/bin/bash

maxThread=16
runPerThread=1

#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo vacation >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./synthetic_bench -n10 -q40 -u50 -r10000 -t100000 -c$nthread 			
		printf "1 " >> results_analysis.txt
		nice -20  ./synthetic_bench -n10 -q40 -u50 -r10000 -t100000 -c$nthread >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
