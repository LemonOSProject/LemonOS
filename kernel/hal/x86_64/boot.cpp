#include "limine.h"

#include "serial.h"

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
}
