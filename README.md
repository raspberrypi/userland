This repository contains the source code for the ARM side libraries used on Raspberry Pi.
These typically are installed in /opt/vc/lib and includes source for the ARM side code to interface to:
EGL, mmal, GLESv2, vcos, openmaxil, vchiq_arm, bcm_host, WFC, OpenVG.

Use buildme to build. It requires cmake to be installed and an arm cross compiler. It is set up to use this one:
https://github.com/raspberrypi/tools/tree/master/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian

To build support for the Wayland winsys in EGL, execute the buildme script like this:

$ BUILD_WAYLAND=1 ./buildme.
