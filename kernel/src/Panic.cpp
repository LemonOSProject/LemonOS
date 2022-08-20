#include <Video/Video.h>

#include <APIC.h>
#include <CString.h>
#include <IDT.h>
#include <Logging.h>

void KernelPanic(const char** reasons, int reasonCount) {
    asm volatile("cli");

    APIC::Local::SendIPI(0, ICR_DSH_OTHER, ICR_MESSAGE_TYPE_FIXED, IPI_HALT);

    video_mode_t v = Video::GetVideoMode();
    Video::DrawRect(0, 0, v.width, v.height, 0, 0, 0);
    int pos = 5;
    for (int i = 0; i < reasonCount; i++) {
        Video::DrawString(reasons[i], 5, pos, 255, 128, 0);
        pos += 10;
    }

    Video::DrawString("Fatal Exception", v.width / 2 - strlen("Fatal Exception") * 16 / 2, v.height / 2, 255, 255, 255, 3, 2);

    Video::DrawString("Lemon has encountered a fatal error.", 0, v.height - 200, 255, 255, 255);
    Video::DrawString("The system has been halted.", 0, v.height - 200 + 8, 255, 255, 255);

    if (Log::console) {
        Log::console->Update();
    }

    asm volatile("hlt");
}

void PrintReason(const video_mode_t& v, int& pos, const char* reason) {
    Video::DrawString(reason, v.width / 2 - strlen(reason) * 8 / 2, pos, 255, 0, 0);
    pos += 10;
}