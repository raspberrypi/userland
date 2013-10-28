#include <stdio.h>
#include <memory.h>
#include "FirePiCam.h"

/**
 * main
 */
int main(int argc, const char **argv)
{
  char filename[100];

  fprintf(stderr, "%s version %d.%d\n", basename(argv[0]), FirePiCam_VERSION_MAJOR, FirePiCam_VERSION_MINOR);
  int status = firepicam_create(argc, argv);
  int frame;

  for (frame = 0; status == 0 && frame<=20; frame++) {
    JPG_Buffer buffer;
    buffer.pData = NULL;
    buffer.length = 0;
    
    status = firepicam_acquireImage(&buffer);

    int fileIndex = frame % 10;
    if (buffer.length > 0) {
      sprintf(filename, "camcv%d.jpg", fileIndex);
      FILE * fJPG = fopen(filename, "w");
      fwrite(buffer.pData, 1, buffer.length, fJPG);
      fflush(fJPG);
      fclose(fJPG);
      firepicam_print_elapsed();
      fprintf(stderr, "%x %dB => %s\n", buffer.pData, buffer.length, filename);
    }
  } // end for (frame)

  firepicam_destroy(status);
}
