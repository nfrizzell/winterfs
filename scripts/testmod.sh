USERNAME=$(whoami)
TEST_FILE=/home/${USERNAME}/wmnt
MOUNT_DIR=/home/${USERNAME}/wmnt
LOOP_DEV=/dev/loop0

make
sudo make install
sudo umount ${MOUNT_DIR} || true
sudo rmmod -f winterfs || true
sudo modprobe winterfs
sudo mount -t winterfs ${LOOP_DEV} ${TEST_FILE}
