#include "limine.h"

#include "serial.h"
#include "string.h"

LIMINE_BASE_REVISION(1);

struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

void limine_init();

extern "C"
void entry() {
    limine_init();

    asm volatile("cli");
    asm volatile("hlt");
}

void limine_init() {
    serial::init();

    serial::debug_write_string("starting lemon...\n");

    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        serial::debug_write_string("Unsupported Limine revision!\n");
    }

    limine_framebuffer *fb = nullptr;
    if (framebuffer_request.response
            && framebuffer_request.response->framebuffer_count >= 1) {
        fb = framebuffer_request.response->framebuffers[0];
    }

    if (!fb) {
        serial::debug_write_string("No framebuffer found!\n");
    } else for (size_t i = 0; i < fb->width * fb->height; i++) {
        ((uint32_t*)fb->address)[i] = 0xff558888;
    }
}
