# Inizializzo le variabili relative ai parametri passati in ingresso allo script

#!/bin/bash

maxThread=32
runPerThread=2

#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo labyrinth >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./labyrinth -i inputs/random-x32-y32-z3-n64.txt -t$nthread 			
		nice -20 ./labyrinth -i inputs/random-x32-y32-z3-n64.txt -t$nthread >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo labyrinth >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./labyrinth -i inputs/random-x32-y32-z3-n96.txt -t$nthread 			
		nice -20 ./labyrinth -i inputs/random-x32-y32-z3-n96.txt -t$nthread >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo labyrinth >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./labyrinth -i inputs/random-x64-y64-z3-n64.txt -t$nthread 			
		nice -20 ./labyrinth -i inputs/random-x64-y64-z3-n64.txt -t$nthread >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo labyrinth >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./labyrinth -i inputs/random-x32-y64-z5-n48.txt -t$nthread 			
		nice -20 ./labyrinth -i inputs/random-x32-y64-z5-n48.txt -t$nthread >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done
