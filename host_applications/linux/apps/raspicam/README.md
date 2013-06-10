RaspiCam Documentation
======================

This document describes the use of the three Raspberry Pi camera applications as of March 2013.

There are three applications provided, `RaspiStill`, `RapiVid` and `RaspiStillYUV`. `RaspiStill` and `RaspiStillYUV` are very similar and are intended for capturing images, `RaspiVid` is for capturing video.

All the applications are command line driven, written to take advantage of the mmal API which runs over OpenMAX. The mmal API provides an easier to use system than that presented by OpenMAX. Note that mmal is a Broadcom specific API used only on Videocore 4 systems.

The applications use up to three OpenMAX(mmal) components - camera, preview and encoder. All applications use the camera component, `RaspiStill` uses the Image Encode component, RaspiVid uses the Video Encode component and `RaspiStillYUV`
does not use an encoder, and sends its YUV output direct from camera component to file.

The preview display is optional, but can be use full screen or directed to specific rectangular area on the display.

In addition it is possible to omit the filename option, in which case the preview is displayed but no file is written.

Command line help is available by typing just the application name in on the command line.


Common Command line Options
===========================

Preview Window
==============

    --preview,  -p          | Preview window settings <'x,y,w,h'>

Allows the user to define the size and location on the screen that the preview window will be placed. Note this will be superimposed over the top of any other windows/graphics.

    --fullscreen,  -f       | Fullscreen preview mode

Forces the preview window to use the whole screen. Note that the aspect ratio of the incoming image will be retained, so there may be bars on some edges.

    --nopreview,  -n,       | Do not display a preview window

Disables the preview window completely. Note that even though the preview is disabled, the camera will still be producing frames, so will be using power.

Camera Control Options
======================

    --sharpness,  -sh       | Set image sharpness (-100 to 100)

Set the sharpness of the image, 0 is the default.

    --contrast,  -co        | Set image contrast (-100 to 100)

Set the contrast of the image, 0 is the default

    --brightness,  -br      | Set image brightness (0 to 100)

Set the brightness of the image, 50 is the default. 0 is black, 100 is white.

    --saturation,  -sa      | Set image saturation (-100 to 100)

set the colour saturation of the image. 0 is the default.

    --ISO,  -ISO            | Set capture ISO

Not yet implemented

    --vstab,  -vs           | Turn on video stabilisation

In video mode only, turn on video stabilisation.

    --ev,  -ev              | Set EV compensation

Set the EV compensation of the image. Range is -10 to +10, default is 0.

    --exposure,  -ex        | Set exposure mode 

Possible options are: 

        off          
        auto          Use automatic exposure mode
        night         Select setting for night shooting
        nightpreview 
        backlight     Select setting for back lit subject
        spotlight    
        sports        Select setting for sports (fast shutter etc)
        snow          Select setting optimised for snowy scenery
        beach         Select setting optimised for beach
        verylong      Select setting for long exposures
        fixedfps      Constrain fps to a fixed value
        antishake     Antishake mode
        fireworks     Select settings

Note that not all of these settings may be implemented, depending on camera tuning.

    --awb,  -awb              | Set Automatic White Balance (AWB) mode
    
Possible options are: 

        off           Turn off white balance calculation
        auto          Automatic mode (default)
        sun           Sunny mode
        cloud         Cloudy mode
        shade         Shaded mode
        tungsten      Tungsten lighting mode
        fluorescent   Fluorescent lighting mode
        incandescent  Incandescent lighting mode
        flash         Flash mode
        horizon       Horizon mode

Set an effect to be applied to the image

    --imxfx,     -ifx         | Set image effect

Possible options are: 

        none           NO effect (default)
        negative       Negate the image
        solarise       Solarise the image
        posterize      Posterise the image
        whiteboard     Whiteboard effect
        blackboard     Blackboard effect
        sketch         Sketch style effect
        denoise        Denoise the image
        emboss         Emboss the image
        oilpaint       Apply an oil paint style effect
        hatch          Hatch sketch style
        gpen          
        pastel         A pastel style effect
        watercolour    A watercolour style effect
        film           Film grain style effect
        blur           Blur the image
        saturation     Colour saturate the image
        colourswap     Not fully implemented
        washedout      Not fully implemented
        posterise      Not fully implemented
        colourpoint    Not fully implemented
        colourbalance  Not fully implemented
        cartoon        Not fully implemented

Colour control

    --colfx,  -cfx          | Set colour effect <U:V>

The supplied U and V parameters (range 0 to 255) are applied to the U and Y channels of the image. For example, --colfx 128:128 should result in a monochrome image.

    --metering,  -mm        | Set metering mode

Specify the metering mode used for the preview and capture

        average   Average the whole frame for metering.
        spot      Spot metering
        backlit   Assume a backlit image
        matrix    Matrix metering


Application specific settings
=============================

RaspiStill
----------

    --width,  -w            | Set image width <size>
    --height,  -h           | Set image height <size>
    --quality,  -q          | Set jpeg quality <0 to 100>

Quality 100 is almost completely uncompressed. 75 is a good all round value

    --raw,  -r              | Add raw bayer data to jpeg metadata

This option inserts the raw Bayer data from the camera in to the JPEG metadata

    --output,  -o           | Output filename <filename>.

Specify the output filename. If not specified, no file is saved

    --verbose,  -v          | Output verbose information during run

Outputs debugging/information messages during the program run.

    --timeout,  -t          | Time before takes picture and shuts down.

The program will run for this length of time, then take the capture (if output is specified). If not specified, this is set to 5 seconds

    --timelapse,  -tl       | Timelapse mode.

The specific value is the time between shots in milliseconds. Note you should specify `%d` at the point in the filename where you want a frame count number to appear. e.g.

    -t 30000 -tl 2000 -o image%d.jpg

will produce a capture every 2 seconds, over a total period of 30s, named `image1.jpg`, `image2.jpg..image15.jpg`.

    --thumb,  -th           | Set thumbnail parameters (x:y:quality)

Allows specification of the thumbnail image inserted in to the JPEG file. If not specified, defaults are a size of 64x48 at quality 35.

    --demo,   d             | Run a demo mode <milliseconds>

This options cycles through range of camera options, no capture is done, the demo will end at the end of the timeout period, irrespective of whether all the options have been cycled. The time between cycles should be specified as a millisecond value.

    --encoding,  -e         | Encoding to use for output file

Valid options are jpg, bmp, gif and png. Note that unaccelerated image types (gif, png, bmp) will take much longer to save than JPG which is hardware accelerated. Also note that the filename suffix is completely ignored when encoding a file.

    --exif,  -x             | EXIF tag to apply to captures (format as 'key=value')

Allows the insertion of specific exif tags in to the JPEG image. You can have up to 32 exif tge entries. This is useful for things like adding GPS metadata. For example, to set the Longitude

    --exif GPS.GPSLongitude=5/1,10/1,15/100

would set the Longitude to 5degs, 10 minutes, 15 seconds. See exif documentation for more details on the range of tags available; the supported tags are as follows.

        IFD0.<   or 
        IFD1.<ImageWidth, ImageLength, BitsPerSample, Compression,
              PhotometricInterpretation, ImageDescription, Make, Model, StripOffsets,
              Orientation, SamplesPerPixel, RowsPerString, StripByteCounts,
              Xresolution, Yresolution, PlanarConfiguration, ResolutionUnit,
              TransferFunction, Software, DateTime, Artist, WhitePoint,
              PrimaryChromaticities, JPEGInterchangeFormat,
              JPEGInterchangeFormatLength, YcbCrCoefficients, YcbCrSubSampling,
              YcbCrPositioning, ReferenceBlackWhite, Copyright>

        EXIF.<ExposureTime, FNumber, ExposureProgram, SpectralSensitivity,
              ISOSpeedRatings, OECF, ExifVersion, DateTimeOriginal, DateTimeDigitized,
              ComponentsConfiguration, CompressedBitsPerPixel, ShutterSpeedValue,
              ApertureValue, BrightnessValue, ExposureBiasValue, MaxApertureValue,
              SubjectDistance, MeteringMode, LightSource, Flash, FocalLength,
              SubjectArea, MakerNote, UserComment, SubSecTime, SubSecTimeOriginal,
              SubSecTimeDigitized, FlashpixVersion, ColorSpace, PixelXDimension,
              PixelYDimension, RelatedSoundFile, FlashEnergy, SpacialFrequencyResponse,
              FocalPlaneXResolution, FocalPlaneYResolution, FocalPlaneResolutionUnit,
              SubjectLocation, ExposureIndex, SensingMethod, FileSource, SceneType,
              CFAPattern, CustomRendered, ExposureMode, WhiteBalance, DigitalZoomRatio,
              FocalLengthIn35mmFilm, SceneCaptureType, GainControl, Contrast,
              Saturation, Sharpness, DeviceSettingDescription, SubjectDistanceRange,
              ImageUniqueID>

        GPS.<GPSVersionID, GPSLatitudeRef, GPSLatitude, GPSLongitudeRef, GPSLongitude,
             GPSAltitudeRef, GPSAltitude, GPSTimeStamp, GPSSatellites, GPSStatus,
             GPSMeasureMode, GPSDOP, GPSSpeedRef, GPSSpeed, GPSTrackRef, GPSTrack,
             GPSImgDirectionRef, GPSImgDirection, GPSMapDatum, GPSDestLatitudeRef,
             GPSDestLatitude, GPSDestLongitudeRef, GPSDestLongitude, GPSDestBearingRef,
             GPSDestBearing, GPSDestDistanceRef, GPSDestDistance, GPSProcessingMethod,
             GPSAreaInformation, GPSDateStamp, GPSDifferential>

        EINT.<InteroperabilityIndex, InteroperabilityVersion, RelatedImageFileFormat,
              RelatedImageWidth, RelatedImageLength>

Note that a small subset of these tags will be set automatically by the camera system, but will be overridden by any exif options on the command line.


RaspiVid
--------

    --width,  -w            | Set image width <size>

Width of resulting video. This should be between 64 and 1920.

    --height,  -h           | Set image height <size>

Height of resulting video. This should be between 64 and 1080.

    --bitrate,  -b          | Set bitrate. 

Use bits per second, so 10MBits/s would be -b 10000000. For H264, 1080p a high quality bitrate would be 15Mbits/s or more.

    --output,  -o           | Output filename <filename>.

Specify the output filename. If not specified, no file is saved

    --verbose,  -v          | Output verbose information during run

Outputs debugging/information messages during the program run.

    --timeout,  -t          | Time before takes picture and shuts down.

The program will run for this length of time, then take the capture (if output is specified). If not specified, this is set to 5seconds

    --demo,  -d             | Run a demo mode <milliseconds>

This options cycles through range of camera options, no capture is done, the demo will end at the end of the timeout period, irrespective of whether all the options have been cycled. The time between cycles should be specified as a millisecond value.

    --framerate,  -fps      | Specify the frames per second to record

At present, the minimum frame rate allowed is 2fps, the maximum is 30fps. This is likely to change in the future.

    --penc,  -e             | Display preview image *after* encoding

Switch on an option to display the preview after compression. This will show any compression artefacts in the preview window. In normal operation, the preview will show the camera output prior to being compressed. This option is not guaranteed to work in future releases.



Examples
========

Still captures
--------------

By default, captures are done at the highest resolution supported by the sensor.  This can be changed using the -w and -h command line options.

Taking a default capture after 2s (note times are specified in milliseconds) on viewfinder, saving in image.jpg

    ./RaspiStill -t 2000 -o image.jpg

Take a capture at a different resolution.

    ./RaspiStill -t 2000 -o image.jpg -w 640 -h 480

Now reduce the quality considerably to reduce file size

    ./RaspiStill -t 2000 -o image.jpg -q 5

Force the preview to appear at coordinate 100,100, with width 300 and height 200 pixels.

    ./RaspiStill -t 2000 -o image.jpg -p 100,100,300,200

Disable preview entirely

    ./RaspiStill -t 2000 -o image.jpg -n

Save the image as a png file (lossless compression, but slower than JPEG). Note that the filename suffix is ignored when choosing the image encoding. 

    ./RaspiStill -t 2000 -o image.png –e png 

Add some EXIF information to the JPEG. This sets the `Artist` tag name to `Boris`, and the GPS altitude to 123.5m. Note that if setting GPS tags you should set as a minimum `GPSLatitude`, `GPSLatitudeRef`, `GPSLongitude`, `GPSLongitudeRef`, `GPSAltitude` and `GPSAltitudeRef`.

    ./RaspiStill -t 2000 -o image.jpg -x IFDO.Artist=Boris -x GPS.GPSAltitude=1235/10

Set an emboss style image effect

    ./RaspiStill -t 2000 -o image.jpg -ifx emboss

Set the U and V channels of the YUV image to specific values (128:128 produces a greyscale image)

    ./RaspiStill -t 2000 -o image.jpg -cfx 128:128

Run preview ONLY for 2s, no saved image.

    ./RaspiStill -t 2000 

Take timelapse picture, one every 10 seconds for 10 minutes (10 minutes = 600000ms), named `image_number_1_today.jpg`, `image_number_2_today.jpg` onwards.

    ./RaspiStill -t 600000 -tl 10000 -o image_num_%d_today.jpg


Video Captures
--------------

Image size and preview settings are the same as for stills capture. Default size for video recording is 1080p (1920x1080)

Record a 5s clip with default settings (1080p30)

    ./RaspiVid -t 5000 -o video.h264

Record a 5s clip at a specified bitrate (3.5MBits/s)

    ./RaspiVid -t 5000 -o video.h264 -b 3500000

Record a 5s clip at a specified framerate (5fps)

    ./RaspiVid -t 5000 -o video.h264 -f 5


