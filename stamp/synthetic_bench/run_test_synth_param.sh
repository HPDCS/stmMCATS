# Inizializzo le variabili relative ai parametri passati in ingresso allo script

#!/bin/bash

maxThread=8
runPerThread=1

#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo synthetic_benc >> results_analysis.txt
maxTxs=1
while [ $maxTxs -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./synthetic_bench  -n20 -q100 -u40 -r100000 -t100000 -c$maxThread 			
		printf "TX_PER_CYCLE=10000 INITIAL_MAX_ADMITTED_TX=$maxTxs" > mcats_conf.txt
		nice -20  ./synthetic_bench -n20 -q100 -u40 -r100000 -t100000 -c$maxThread >> results_analysis.txt
		k=$[$k+1]
	done
	maxTxs=$[$maxTxs+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
