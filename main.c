#include "mymod.h"

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <sys/mman.h>

#define WIDTH 1024
#define HEIGHT 768
#define PIXEL_SIZE 4

struct u_kyouko_device {
   unsigned int *u_control_base;
   unsigned int *u_frame_base;
} kyouko3;

unsigned int U_READ_REG(unsigned int rgister) {
   return *(kyouko3.u_control_base + (rgister >> 2));
}

void U_WRITE_FB(int i, unsigned int color) {
   *(kyouko3.u_frame_base + i)  = color;
}

int main(void) {
   int fd;
   unsigned int result;
   int i;

   // Scary way of setting up a command (without needing a structure).
   unsigned long command;
   command = FIFO_QUEUE;
   command = command << 32;
   command += 0x3;

   fd = open("/dev/kyouko3", O_RDWR);
   kyouko3.u_control_base =
      mmap(0, CONTROL_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
   kyouko3.u_frame_base =
      mmap(
         0,
         WIDTH * HEIGHT * PIXEL_SIZE,
         PROT_READ|PROT_WRITE,
         MAP_SHARED, fd, 0x80000000);

   result = U_READ_REG(DeviceVRAM);
   printf("Ram size in MB is: %d\n", result);

   ioctl(fd, VMODE, GRAPHICS_ON); // graphics mode on

   for(i = 200*1024; i < 201*1024; ++i)
      U_WRITE_FB(i, 0xff0000); 
 
   ioctl(fd, FIFO_QUEUE, &command); // write zero to flush register
   ioctl(fd, FIFO_FLUSH, 0); // advances tail to head
  
   sleep(5);

   ioctl(fd, VMODE, GRAPHICS_OFF); // graphics mode off

   close(fd);
   return 0;
}
