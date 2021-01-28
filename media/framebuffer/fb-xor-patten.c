#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

int
main()
{
    int fd;
    struct fb_var_screeninfo screeninfo;
    uint32_t *data;
    uint32_t x, y, width, height, xor;

    fd = open("/dev/fb0", O_RDWR);
    if (fd == -1)
    {
        perror("open /dev/fb0");
        exit(EXIT_FAILURE);
    }

    ioctl(fd, FBIOGET_VSCREENINFO, &screeninfo);
    if (screeninfo.bits_per_pixel != 32)
    {
        fprintf(stderr, "Expected 32 bits per pixel\n");
        exit(EXIT_FAILURE);
    }

    width = screeninfo.xres;
    height = screeninfo.yres;

    data = (uint32_t *)mmap(0, width * height * 4, PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd, 0);
    if (data == MAP_FAILED)
    {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            xor = (x ^ y) % 256;
            data[y * width + x] = (xor << 16) | (xor << 8) | xor;
        }
    }

    exit(EXIT_SUCCESS);
}
