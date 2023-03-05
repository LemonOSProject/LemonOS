#include <lemon/core/Framebuffer.h>

#include <lemon/abi/framebuffer.h>

#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

namespace Lemon {

long CreateFramebufferSurface(Surface* surface) {
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        perror("open");
		return -1;
    }

    FramebufferInfo fbInfo;
    if (ioctl(fd, LeIoctlGetFramebuffferInfo, &fbInfo)) {
		perror("ioctl");
		return -1;
	}

	assert(fbInfo.bpp == 32);

    void* p = mmap(nullptr, fbInfo.width * fbInfo.pitch, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(p == MAP_FAILED) {
		perror("framebuffer mmap");
		return -1;
	}

    surface->width = fbInfo.width;
    surface->height = fbInfo.height;
    surface->buffer = (uint8_t*)p;

    return 0;
}

}
