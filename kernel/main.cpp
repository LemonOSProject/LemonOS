#include <tests/tests.h>

#include <thread/thread.h>

#include <boot.h>
#include <clock.h>
#include <logging.h>
#include <tar.h>
#include <string.h>

void kernel_test() {
    tests::run();

    thread::current()->kill();
}

void kernel_main() {
    assert(hal::cpu::interrupt_flag());
    log_info("hello from main");

    auto *test_thread = thread::create_kernel_thread("kernel_test", kernel_test, 0x10000, 0);

    BootModule *initrd_module = nullptr;
    log_info("initrd_module: {:x}", initrd_module);

    hal::boot::enumerate_boot_modules([&initrd_module](BootModule *mod) {
        auto *s = strstr(mod->path, "initrd.tar");
        log_info("{}", (const char*)s);
        if (strlen(s) == (sizeof "initrd.tar") - 1) {
            // Found initrd.tar
            initrd_module = mod;
        }
    });
    log_info("initrd_module: {:x}", initrd_module);

    if (!initrd_module) {
        lemon_panic("Failed to load initrd.tar");
    }

    TarArchive tar{initrd_module->base, initrd_module->size};

    void *data;
    size_t len;

    if(tar.extract_file("system", &data, &len)) {
        lemon_panic("Failed to load file 'system' from initrd.tar");
    }

    log_info("file: {} ({} bytes)", "system", len);

    for(;;) ;
}
