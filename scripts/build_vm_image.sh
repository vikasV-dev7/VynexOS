#!/bin/bash
set -e

echo "========================================"
echo "VynexOS VM Image Generation Script"
echo "========================================"

# 1. Configure
echo "[1/4] Configuring CMake (C++23)..."
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 2. Build
echo "[2/4] Building VynexOS user-space binaries..."
make -j$(nproc)

# 3. Image Generation
echo "[3/4] Generating bootable VM image (ext4 rootfs)..."
IMAGE_NAME="vynexos_dev.img"
# Create a 512MB empty image
dd if=/dev/zero of=$IMAGE_NAME bs=1M count=512
mkfs.ext4 $IMAGE_NAME

# Mount and copy binaries (requires sudo or fakeroot/guestfish in real CI, using mock steps here)
mkdir -p mnt
# sudo mount -o loop $IMAGE_NAME mnt
# sudo cp -r bin/* mnt/usr/bin/
# sudo cp -r lib/* mnt/usr/lib/
# sudo umount mnt
echo "Image generated: $IMAGE_NAME"

# 4. Boot
echo "[4/4] Generating QEMU boot script..."
cat << 'EOF' > boot_qemu.sh
#!/bin/bash
echo "Booting VynexOS in QEMU..."
qemu-system-x86_64 -m 2G -smp 2 \
    -kernel /boot/vmlinuz-linux \
    -drive file=vynexos_dev.img,format=raw \
    -append "root=/dev/sda rw init=/usr/bin/vynex_init console=ttyS0" \
    -nographic \
    -device virtio-net-pci,netdev=net0 \
    -netdev user,id=net0,hostfwd=tcp::2222-:22
EOF
chmod +x boot_qemu.sh

echo "Done. Run './build/boot_qemu.sh' to boot the VM."
