USERNAME=$(whoami)
TEST_FILE=/home/${USERNAME}/diskimg
MOUNT_DIR=/home/${USERNAME}/wmnt
LOOP_DEV=/dev/loop16

make
sudo make install
sudo losetup ${LOOP_DEV} ${TEST_FILE} || true
sudo umount ${MOUNT_DIR} || true
sudo rmmod -f winterfs || true
sudo modprobe winterfs
sudo mount -t winterfs ${LOOP_DEV} ${MOUNT_DIR}
