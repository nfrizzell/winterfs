TEST_FILE=/home/user1/diskimg

sudo -u root sh -c "cd /sys/kernel/tracing; echo function_graph > current_tracer echo 1 > tracing_on"
cat ${TEST_FILE}
sudo -u root sh -c "cd /sys/kernel/tracing; echo 0 > tracing_on; less trace"
