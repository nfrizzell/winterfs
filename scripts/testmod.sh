USERNAME=$(whoami)
TEST_FILE=/home/${USERNAME}/diskimg
MOUNT_DIR=/home/${USERNAME}/wmnt
MKFS_PATH=../mkfs.winterfs/a.out
LOOP_DEV=/dev/loop7

make
sudo make install
dd if=/dev/zero of=${TEST_FILE} bs=4096 count=256
sudo losetup ${LOOP_DEV} ${TEST_FILE} || true
sudo ./${MKFS_PATH} ${LOOP_DEV}
sudo umount ${MOUNT_DIR} || true
sudo rmmod -f winterfs || true
sudo modprobe winterfs
sudo mount -t winterfs ${LOOP_DEV} ${MOUNT_DIR}
sudo chown ${USERNAME}:${USERNAME} ${MOUNT_DIR}
