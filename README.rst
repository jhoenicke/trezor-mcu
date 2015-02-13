TREZOR Firmware
===============

http://bitcointrezor.com/

How to build Trezor firmware?
-----------------------------

1. Install Docker (from docker.com or from your distribution repositories)
2. ``git clone https://github.com/trezor/trezor-mcu.git``
3. ``cd trezor-mcu``
4. ``./firmware-docker-build.sh``

This creates trezor.bin in current directory and prints its fingerprint at the last line of the build log.

How to get fingerprint of firmware signed and distributed by SatoshiLabs?
-------------------------------------------------------------------------

1. Pick version of firmware binary listed on https://mytrezor.com/data/firmware/releases.json
2. Download it: ``wget -O trezor.signed.bin.hex https://mytrezor.com/data/firmware/trezor-1.1.0.bin.hex``
3. ``xxd -r -p trezor.signed.bin.hex trezor.signed.bin``
4. ``./firmware-fingerprint.sh trezor.signed.bin``

Step 4 should produce the same sha256 fingerprint like your local build.

The reasoning for ``firmware-fingerprint.sh`` script is that signed firmware has special header holding signatures themselves, which must be avoided while calculating the fingerprint.

How to build Trezor firmware without docker?
--------------------------------------------

Under Ubuntu 14.04:

1. Install build tools
::
    sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys FE324A81C208C89497EFC6246D1D8367A3421AFB
    sudo sh -c 'echo "deb http://ppa.launchpad.net/terry.guo/gcc-arm-embedded/ubuntu trusty main" >> /etc/apt/sources.list'
    sudo apt-get update
    sudo apt-get install -y build-essential git gcc-arm-none-eabi

2. Clone the repositories libopencm3 and trezor-mcu under the same directory
::
    git clone https://github.com/libopencm3/libopencm3
    git clone https://github.com/trezor/trezor-mcu

3. Build libopencm3
::
    cd libopencm3
    make
    cd ..

4. Build Trezor firmware
::
    cd trezor-mcu
    make
    cd firmware
    make sign
    cd ../..

5. Trezor firmware is now in ``trezor-mcu/firmware/trezor.bin``
