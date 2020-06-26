#include <gui/messagebox.h>

#include <gui/window.h>
#include <gui/widgets.h>
#include <gfx/font.h>

namespace Lemon::GUI
{
	int pressed = 0;
	void OnMessageBoxOKPressed(Lemon::GUI::Button* b){
		pressed = 0;
		b->window->closed = true;
	}

	int DisplayMessageBox(const char* title, const char* message, MsgBoxButtons buttons){
		Window* win = new Window("Open...", {Graphics::GetTextLength(message) + 10, 80}, WINDOW_FLAGS_RESIZABLE, WindowType::GUI);
		
		Label* label = new Label(message, {10, 10, 180, 12});
		win->AddWidget(label);

		Button* okBtn = new Button("OK", {0, 5, 100, 24});
		win->AddWidget(okBtn);
		okBtn->SetLayout(LayoutSize::Fixed, LayoutSize::Fixed, WAlignCentre, WAlignBottom);
		okBtn->OnPress = OnMessageBoxOKPressed;

		bool paint = true;

		while(!win->closed){
			LemonEvent ev;
			while(win->PollEvent(ev)){
				win->GUIHandleEvent(ev);
				paint = true;
			}

			if(paint){
				win->Paint();
				paint = false;
			}
		}

		delete win;

		return pressed;
	}
}