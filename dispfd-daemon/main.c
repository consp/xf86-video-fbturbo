#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/fb.h>

int main() {
  int dispfd = open("/dev/disp", O_RDWR);
  if (dispfd < 0) {
    exit(-1);
  }
  printf("dispfd is now open.\n");

  int fbfd = open("/dev/fb0", O_RDWR);
  printf("fbfd is now open.\n");

  if (fbfd < 0) {
    close(dispfd);
    exit(-1);
  }

  // NOTE: at this moment, the fb console display is restored.
  // however, if the program exits, the fb console display is disabled again...

  close(fbfd);
  printf("fbfd is now closed.\n");

  // NOTE: at this moment, the fb console display is still active.
  // Don't close the dispfd -- it will hold a ref count in the kernel driver,
  // so that when X11 server exits, it does not disable the fb display.
  // See: drivers/video/fbdev/sunxi/disp2/disp/dev_disp.c:L2650:disp_open
  while(true) {
	sleep(1000);
  }
  close(dispfd);
  return 0;
}
