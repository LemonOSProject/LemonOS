#include <video.h>

#include <apic.h>
#include <idt.h>

void KernelPanic(const char** reasons, int reasonCount){
	asm("cli");

	APIC::Local::SendIPI(0, ICR_DSH_OTHER, ICR_MESSAGE_TYPE_FIXED, IPI_HALT);

	video_mode_t v = Video::GetVideoMode();
	Video::DrawRect(0,0,v.width,v.height,255,0,0);
	for(int i = 0; i < reasonCount; i++){
		Video::DrawString(reasons[i], 0,i*8,255,255,255);
	}

	Video::DrawString("Lemon has encountered a fatal error.", 0, v.height - 200, 255, 255, 255);
	Video::DrawString("The system has been halted.", 0, v.height - 200 + 8, 255, 255, 255);
	asm("hlt");
}