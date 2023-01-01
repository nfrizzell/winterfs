ORIG_DIR=$(pwd)
USERNAME=$(whoami)
TEST_FILE=/home/${USERNAME}/wmnt
MOUNT_DIR=/home/${USERNAME}/wmnt
LOOP_DEV=/dev/loop0

cd ${ORIG_DIR}
make
sudo make install
sudo umount ${MOUNT_DIR} || true
sudo rmmod -f winterfs || true

sudo -u root sh -c "cd /sys/kernel/tracing; echo function_graph > current_tracer; echo 1 > tracing_on"

sudo modprobe winterfs
sudo mount -t winterfs ${LOOP_DEV} ${TEST_FILE}

sudo -u root sh -c "cd /sys/kernel/tracing; echo 0 > tracing_on; less trace"
