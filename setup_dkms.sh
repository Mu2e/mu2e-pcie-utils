#!/bin/bash

# This script prepares the repo to serve as a DKMS build area (in /usr/src/mu2e_pcie_utils-VERSION)
# Any code changes to the TRACE, mu2e, or pci_devel_main modules will need to have a either a new version,
# or a dkms -m mu2e_pcie_utils -v VERSION uninstall, remove, add, build, install cycle

# To install (as root):
# cd /usr/src
# export VERSION=<Version number, e.g. 2.08.01>
# git clone https://github.com/Mu2e/mu2e_pcie_utils mu2e_pcie_utils-$VERSION
# cd mu2e_pcie_utils-$VERSION
# chmod +x setup_dkms.sh;./setup_dkms.sh
# dkms add -m mu2e_pcie_utils -v $VERSION
# dkms autoinstall

echo mu2e >/etc/modules-load.d/mu2e.conf
echo 'KERNEL=="mu2e*", MODE="0666"' >/etc/udev/rules.d/98-mu2e.rules

if ! [ -d trace ]; then
  git clone https://github.com/art-daq/trace.git
fi

sed -i "s/#define DRIVER_VERSION_STRING .*/#define DRIVER_VERSION_STRING \"`git describe --tags`\"/g" mu2e_driver/mu2e_main.c

cat >Makefile <<EOF

EXTRA_SYMBOLS=KBUILD_EXTRA_SYMBOLS=$PWD/trace/src_module/Module.symvers
TRACE_INC=$PWD/trace/include

all:
	make -C trace/src_module/
	make -C mu2e_driver/ CC=/usr/bin/gcc EXTRA_SYMBOLS=\${EXTRA_SYMBOLS} TRACE_INC=\${TRACE_INC}
	make -C mcs/basic_driver/ CC=/usr/bin/gcc EXTRA_SYMBOLS=\${EXTRA_SYMBOLS} TRACE_INC=\${TRACE_INC}

clean:
	make -C trace/src_module/ clean
	make -C mu2e_driver/ clean
	make -C mcs/basic_driver/ clean

EOF
