
#!/bin/bash

maxThread=32
runPerThread=1

#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo vacation >> results_analysis.txt
max_tx=2
while [ $max_tx -le $maxThread ] 
        do
        echo "TX_PER_CYCLE=1000 INITIAL_MAX_ADMITTED_TX="$max_tx > mcats_conf.txt
	nthread=1
	while [ $nthread -le $maxThread ] 
        	do
	        k=0
	        while [ $k -lt $runPerThread ]
	                do
	                echo nice -20 ./vacation -n4 -q60 -u90 -r32768 -t148576 -c$nthread                     
	                nice -20 ./vacation -n4 -q60 -u90 -r32768 -t148576 -c$nthread  >> results_analysis.txt
	                k=$[$k+1]
	        done
	        nthread=$[$nthread+1]
	done
	max_tx=$[$max_tx+2]
done

