This repository contains the source code for the ARM side libraries used on Raspberry Pi.
These typically are installed in /opt/vc/lib and includes source for the ARM side code to interface to:
EGL, mmal, GLESv2, vcos, openmaxil, vchiq_arm, bcm_host, WFC, OpenVG.

Use buildme to build. It requires cmake to be installed and an arm cross compiler. It is set up to use this one:
https://github.com/raspberrypi/tools/tree/master/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian

Raspistill shader parameters can be controlled through OpenSoundControl.
To enable that, you need liblo-dev package.
`sudo apt-get install liblo7 liblo-dev`
Install it and run ./buildme to build raspistill with OSC support.
Then you can control each shader parameters with OSC message.

To cross-compile with liblo support, you can download deb package on your pi with this command : 
`apt-get download liblo7 liblo-dev`
Then extract them in the folder returned by : `arm-linux-gnueabihf-gcc -print-sysroot`.

Note that this repository does not contain the source for the edid_parser and vcdbg binaries due to licensing restrictions.
