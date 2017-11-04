#!/bin/bash

if [[ $# != 1 ]]; then
    echo "usa $0 unix_path stat_file"
    exit 1
fi


./client -l $1 -c paperino
./client -l $1 -c pippo
./client -l $1 -c pluto
./client -l $1 -c minni

./client -l $1 -k paperino -R -1 &
pid=$!



for((i=0;i<50;++i)); do 
    ./client -l $1 -k pippo -S "ciao":pluto; 
    ./client -l $1 -k pluto -S "ciao":pippo;   
    ./client -l $1 -k minni -s client:pluto -s client:pippo; 
done


sleep 1

echo "PAPERINO id"
echo $(pidof client)
kill -TERM $pid

echo "Test OK!"
exit 0

