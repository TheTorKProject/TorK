#!/bin/bash
FILE=$1
CLI_PATH=$2
#FILE_FRAMES=$3
TIME=1

if [[ $FILE == "" ]]; then
	echo "Missing output file"
	echo "Usage: ./statspicker.sh outputfile.txt <path_to_cli_socket> [stats.txt]"
	exit
fi

# Clear file if exists
echo -n "" > $1

#if [[ $FILE_FRAMES != "" ]]; then
#	echo -n "" > $FILE_FRAMES
#fi

while [ true ]
do
	#Gathers stats from the cli and waits TIME second
	echo "stats_bytes" | nc -U $CLI_PATH -w $TIME | tee -a $FILE
	#if [[ $FILE_FRAMES != "" ]]; then
	#	echo "stats_frames" | nc -U $CLI_PATH -w 0 | tee -a $FILE_FRAMES
	#fi
	if [ ${PIPESTATUS[1]} -ne 0 ]; then
	    echo "Fail / Terminated"
	    break
	fi
done
