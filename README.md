Update
======

This repo is ancient and deprecated.

It largely contains code using proprietary APIs to interface
to the VideoCore firmware. We have since move to standard linux APIs.

V4L2, DRM/KMS and Mesa are the APIs you should be using.

The few useful tools from here (dtoverlay, dtmerge, vcmailbox, vcgencmd)
have been moved to the raspberrypi/utils repo.

Code from here is no longer installed on latest RPiOS Bookworm images.

If you are using code from here you should rethink your solution.

Consider this repo closed.

========================

This repository contains the source code for the ARM side libraries used on Raspberry Pi.
These typically are installed in /opt/vc/lib and includes source for the ARM side code to interface to:
EGL, mmal, GLESv2, vcos, openmaxil, vchiq_arm, bcm_host, WFC, OpenVG.

Use buildme to build. It requires cmake to be installed and an ARM cross compiler. For 32-bit cross compilation it is set up to use this one:
https://github.com/raspberrypi/tools/tree/master/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian

Whilst 64-bit userspace is not officially supported, some of the libraries will work for it. To cross compile, install gcc-aarch64-linux-gnu and g++-aarch64-linux-gnu first. For both native and cross compiles, add the option ```--aarch64``` to the buildme command.

Note that this repository does not contain the source for the edidparser and vcdbg binaries due to licensing restrictions.
