#include "panic.h"

#include "cpu.h"
#include "serial.h"

#include <le/formatter.h>

#include <video/video.h>

void lemon_panic(const char *reason, hal::cpu::InterruptFrame *frame) {
    serial::debug_write_string("panic: ");
    serial::debug_write_string(reason);
    serial::debug_write_string("\r\n");

    // Draw an orange border because why not
    video::fill_rect(0, 0, video::get_screen_width(), video::get_screen_height(), 0);
    video::fill_rect(0, 0, video::get_screen_width(), 4, 0xffdd8800);
    video::fill_rect(0, video::get_screen_height() - 4, video::get_screen_width(), 4, 0xffdd8800);
    video::fill_rect(0, 4, 4, video::get_screen_height() - 8, 0xffdd8800);
    video::fill_rect(video::get_screen_width() - 4, 4, 4, video::get_screen_height() - 8, 0xffdd8800);

    if (frame) {
        char format_buffer[128];
        format_n(format_buffer, 128, "rip: {:x} rsp: {:x} rbp: {:x}\r\n"
            "cr2: {:x}, cr3: {:x}",
            frame->rip, frame->rsp, frame->rbp,
            hal::cpu::cr2(), hal::cpu::cr3());

        serial::debug_write_string(format_buffer);

        uint64_t* rbp = (uint64_t*)frame->rbp;
        uint64_t rip = 0;
        while(rbp){
            rip = *(rbp + 1);
            rbp = (uint64_t*)(*rbp);

            format_n(format_buffer, 128, "{:x}\r\n", rip);
            serial::debug_write_string(format_buffer);
        }
    }

    asm volatile("cli; hlt;");
}
