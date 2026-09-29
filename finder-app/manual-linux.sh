#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo

set -e
set -u

OUTDIR=/tmp/aeld

KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1

FINDER_APP_DIR=$(realpath $(dirname $0))

ARCH=arm64
CROSS_COMPILE=aarch64-linux-gnu-

if [ $# -lt 1 ]
then
    echo "Using default directory ${OUTDIR} for output"
else
    OUTDIR=$1
    echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd ${OUTDIR}

if [ ! -d "${OUTDIR}/linux-stable" ]
then
    echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"

    git clone ${KERNEL_REPO} \
        --depth 1 \
        --single-branch \
        --branch ${KERNEL_VERSION}
fi

if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]
then
    cd linux-stable

    echo "Checking out version ${KERNEL_VERSION}"

    git checkout ${KERNEL_VERSION}

    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper

    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

    make -j$(nproc) \
        ARCH=${ARCH} \
        CROSS_COMPILE=${CROSS_COMPILE} all
fi

echo "Adding the Image in outdir"

cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/Image

echo "Creating the staging directory for the root filesystem"

cd ${OUTDIR}

if [ -d "${OUTDIR}/rootfs" ]
then
    echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"

    sudo rm -rf ${OUTDIR}/rootfs
fi

mkdir -p ${OUTDIR}/rootfs

cd ${OUTDIR}/rootfs

mkdir -p \
bin \
dev \
etc \
home \
lib \
lib64 \
proc \
sbin \
sys \
tmp \
usr/bin \
usr/lib \
usr/sbin \
var/log

cd "$OUTDIR"

if [ ! -d "${OUTDIR}/busybox" ]
then
    git clone git://busybox.net/busybox.git

    cd busybox

    git checkout ${BUSYBOX_VERSION}

    make distclean

    make defconfig
else
    cd busybox
fi

make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}

make CONFIG_PREFIX=${OUTDIR}/rootfs \
    ARCH=${ARCH} \
    CROSS_COMPILE=${CROSS_COMPILE} \
    install

echo "Library dependencies"

${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter"

${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library"

SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

#cp -a ${SYSROOT}/lib/* ${OUTDIR}/rootfs/lib/ 2>/dev/null || true

#cp -a ${SYSROOT}/lib64/* ${OUTDIR}/rootfs/lib64/ 2>/dev/null || true

LIBDIR=/usr/aarch64-linux-gnu/lib
cp -L ${LIBDIR}/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/
cp -L ${LIBDIR}/libc.so.6 ${OUTDIR}/rootfs/lib/
cp -L ${LIBDIR}/libm.so.6 ${OUTDIR}/rootfs/lib/
cp -L ${LIBDIR}/libresolv.so.2 ${OUTDIR}/rootfs/lib/

sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3

sudo mknod -m 600 ${OUTDIR}/rootfs/dev/console c 5 1

cd ${FINDER_APP_DIR}

make clean

make CROSS_COMPILE=${CROSS_COMPILE}

cp writer ${OUTDIR}/rootfs/home/

cp writer.sh ${OUTDIR}/rootfs/home/

cp finder.sh ${OUTDIR}/rootfs/home/

cp finder-test.sh ${OUTDIR}/rootfs/home/

cp autorun-qemu.sh ${OUTDIR}/rootfs/home/

mkdir -p ${OUTDIR}/rootfs/home/conf

cp conf/assignment.txt ${OUTDIR}/rootfs/home/conf
cp conf/username.txt ${OUTDIR}/rootfs/home/conf

sed -i 's#../conf/assignment.txt#conf/assignment.txt#g' \
${OUTDIR}/rootfs/home/finder-test.sh

cat > ${OUTDIR}/rootfs/init << 'EOF'
#!/bin/sh

mount -t proc proc /proc
mount -t sysfs sysfs /sys

echo "Boot complete"

exec /bin/sh
EOF

chmod +x ${OUTDIR}/rootfs/init

sudo chown -R root:root ${OUTDIR}/rootfs

cd ${OUTDIR}/rootfs

find . | cpio -H newc -ov --owner root:root \
> ${OUTDIR}/initramfs.cpio

gzip -f ${OUTDIR}/initramfs.cpio

echo "Kernel image created at ${OUTDIR}/Image"
echo "Initramfs created at ${OUTDIR}/initramfs.cpio.gz"
