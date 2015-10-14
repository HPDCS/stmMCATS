# Inizializzo le variabili relative ai parametri passati in ingresso allo script

#!/bin/bash

export LEARNING=true
export STM_STATS=true

mkdir results

maxRun=127
runPerThread=1
run=1
array=(0 4 8 12 16 20 24 28 1 5 9 13 17 21 25 29)
while [ $run -lt $maxRun ]
	do
	echo $run
	j=1
	affinity=0
	nthread=$((RANDOM%16+1))
	while [ $j -lt $nthread ]
	do
		affinity=$(echo $affinity,${array[$j]})
	        j=$[$j+1]
	done

	q=$(((RANDOM%60)+10))
	u=$(((RANDOM%100)+50))
	t=2097152
	n=$((RANDOM*32))
	r=$((($n%524288)+8192))
	echo nice -q$q -u$u -r$r -t$t -c$nthread
	nice -20 taskset -c $affinity ./vacation -n4 -q$q -u$u -r$r -t$t -c$nthread > ris0.txt
	cd results
	mkdir $nthread
	cd ..
	mv *.csv ./results/$nthread/
	grep "Time" ris0.txt|sed -E -e "s/[a-zA-Z =:]*//g" > ris1.txt
	exec 4<ris1.txt
	read -u 4 tempo
	echo $nthread,$tempo >> risultati.txt
	run=$[$run+1]
done


#ciclo:
#	Avvio il benchmark
#	intercetto la fine del benchmark
#	faccio il parsing dell'output
#	salvo i risultati su file
#	genero nuovi parametri per il benchmark
#fine ciclo
