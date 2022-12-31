TEST_FILE=/home/user1/diskimg

cd /sys/kernel/tracing
echo function_graph > current_tracer
echo 1 > tracing_on
cat ${TEST_FILE}
echo 0 > tracing_on
less trace
