cnt=0

for (( i=0; i<100; i=i+1 ))
  do
    make clean && make
    cd build
    echo "$cnt / 100"
    make check > test.txt

    output=$( tail -n 1 test.txt )
    output=($output)

    IFS=' ' read -r -a array <<< "${output[0]}"
    failed_cnt=${array[0]}

    if [ $failed_cnt = "All" ]
    then
        cnt=$((cnt+1))
        cd ../
    else
        echo "Fail!!!"
        cd ../
        tar -cvf ../../build_$cnt.tar build
        break
    fi
    
done

rm test.txt

if [ $cnt -eq 100 ]
then
    echo "Successfully passed all 113 tests after 100 repetitions!"
fi