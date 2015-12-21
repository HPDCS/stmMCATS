# Inizializzo le variabili relative ai parametri passati in ingresso allo script

#!/bin/bash

maxThread=32
runPerThread=1

#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo yada >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./yada/yada -a10 -i yada/inputs/ttimeu10000.2 -t$nthread 			
		nice -20 ./yada/yada -a10 -i yada/inputs/ttimeu10000.2 -t$nthread >> results_analysis.txt
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
		echo nice -20 ./vacation/vacation -n4 -q60 -u90 -r1048576 -t4096 -c$nthread 			
		nice -20 ./vacation/vacation -n4 -q60 -u90 -r1048576 -t4096 -c$nthread >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo ssca2 >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./ssca2/ssca2 -s14 -i1.0 -u1.0 -l9 -p9 -t$nthread 			
		 nice -20 ./ssca2/ssca2 -s14 -i1.0 -u1.0 -l9 -p9 -t$nthread  >> results_analysis.txt
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
		echo nice -20 ./labyrinth/labyrinth -i labyrinth/inputs/random-x48-y48-z3-n64.txt -t$nthread 			
		 nice -20 ./labyrinth/labyrinth -i labyrinth/inputs/random-x48-y48-z3-n64.txt -t$nthread  >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo bayes >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./bayes/bayes -v32 -r4096 -n2 -p20 -i2 -e2 -t$nthread 			
		nice -20 ./bayes/bayes -v32 -r4096 -n2 -p20 -i2 -e2 -t$nthread >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo genome >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./genome/genome -g512 -s32 -n32768 -t$nthread 			
		nice -20 ./genome/genome -g512 -s32 -n32768 -t$nthread >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo intruder >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./intruder/intruder -a10 -l16 -n4096 -s1 -t$nthread 			
		nice -20 ./intruder/intruder -a10 -l16 -n4096 -s1 -t$nthread >> results_analysis.txt
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
		echo nice -20 ./kmeans/kmeans -m15 -n15 -t0.05 -i kmeans/inputs/random-n16384-d24-c16.txt -p$nthread 			
		nice -20 ./kmeans/kmeans -m15 -n15 -t0.05 -i kmeans/inputs/random-n16384-d24-c16.txt -p$nthread >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------

