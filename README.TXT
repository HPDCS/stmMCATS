*Introduction*
MCATS (Markov Chain-based Adaptive Transaction Scheduler) is an adaptive transaction scheduler for Transactional Memory (TM). The objective of MCATS is the one of maximizing the system throughput by dynamically regulating the maximum number of concurrent transactions admitted to run along the lifetime of the STM application. MCATS exploits a TM performance model based on Markov Chain. 

*Compile*
- You can use "compile.sh" to compile TinySTM (with MCATS) and three applications of STAMP (Intruder, Yada and Vacation)

*Run*
- To run an application, in addition to the input parameters required by the application (see STAMP documentation), the following ones are required by MCATS:
	-z: length of the observation window in terms of number of executed transactions 
	-y: initial number of concurrent transactions admitted by the scheduler
	-w: number of tx classes (currenlty only one class is supported!)
	
Examples:

1) the following command
"./intruder -a10 -l128 -n262144 -s1 -t8 -z1000 -y8 -w1"
runs Intruder with:
	- observation window=1000
	- initial number of concurrent transactions equal to 8
	- number of tx classes equal to 1

2) the following command
"./yada -a15 -i inputs/ttimeu100000.2 -t16 -z20000 -y6 -w1"
runs Yada with:
	- observation window=20000
	- initial number of concurrent transactions equal to 6
	- number of tx classes equal to 1
	
3) the following command
"./vacation -n2 -q60 -u90 -r32768 -t1048576 -c4 -z5000 -y2 -w1"
runs Yada with:
	- observation window=5000
	- initial number of concurrent transactions equal to 2
	- number of tx classes equal to 1	
	
