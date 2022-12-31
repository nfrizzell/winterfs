pwd
make
sudo make install
sudo umount ~/wmnt || true
sudo rmmod -f winterfs || true
sudo modprobe winterfs
sudo mount -t winterfs /dev/loop0 ~/wmnt
