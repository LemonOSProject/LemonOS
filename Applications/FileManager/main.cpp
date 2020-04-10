#include <lemon/types.h>
#include <lemon/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gfx/window/window.h>
#include <lemon/keyboard.h>
#include <lemon/ipc.h>
#include <stdlib.h>
#include <list.h>
#include <lemon/filesystem.h>
#include <fcntl.h>
#include <unistd.h>
#include <gfx/window/messagebox.h>
#include <lemon/spawn.h>

extern "C"
int main(char argc, char** argv){
	win_info_t windowInfo;
	Window* window;

	windowInfo.width = 512;
	windowInfo.height = 256;
	windowInfo.x = 100;
	windowInfo.y = 50;
	windowInfo.flags = 0;
	strcpy(windowInfo.title, "FileMan");

	window = CreateWindow(&windowInfo);

	ListView* fileList = new ListView((rect_t){{0,20},{windowInfo.width,windowInfo.height - 20}},20);
	
	window->widgets.add_back(fileList);

	char* currentPath = (char*)malloc(512);
	strcpy(currentPath, "/");

	int currentDir = open(currentPath, 666);
	int i = 0;
	lemon_dirent_t dirent;
	while(lemon_readdir(currentDir, i++, &dirent)){
		fileList->contents.add_back(new ListItem(dirent.name));
	}

	Label* pathLabel = new Label("test", {{4,4},{508, 24}});
	window->widgets.add_back(pathLabel);

	fileList->ResetScrollBar();

	for(;;){
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if (msg.msg == WINDOW_EVENT_CLOSE){
				DestroyWindow(window);
				exit(0);
			} else if(msg.msg == WINDOW_EVENT_MOUSEDOWN){
				uint32_t mouseX;
				uint32_t mouseY;
				mouseX = (msg.data >> 32);
				mouseY = (uint32_t)msg.data & 0xFFFFFFFF;
				HandleMouseDown(window, {(int)mouseX, (int)mouseY});
			}
			else if(msg.msg == WINDOW_EVENT_MOUSEUP){	
				uint32_t mouseX;
				uint32_t mouseY;
				mouseX = msg.data >> 32;
				mouseY = (uint32_t)msg.data & 0xFFFFFFFF;
				HandleMouseUp(window, {(int)mouseX, (int)mouseY});
			} else if(msg.msg == WINDOW_EVENT_KEY){
				if(msg.data == KEY_ARROW_DOWN){
					fileList->selected++;
					if(fileList->selected >= fileList->contents.get_length()){
						fileList->selected = fileList->contents.get_length() - 1;
					}
				} else if(msg.data == KEY_ARROW_UP){
					fileList->selected--;
					if(fileList->selected < 0){
						fileList->selected = 0;
					}
				} else if(msg.data == '\n'){
					ListItem* item = fileList->contents.get_at(fileList->selected);
					lemon_readdir(currentDir, fileList->selected, &dirent);
					if(dirent.type & FS_NODE_DIRECTORY){
						close(currentDir);

						strcpy(currentPath + strlen(currentPath), "/");
						strcpy(currentPath + strlen(currentPath), dirent.name);

						currentDir = lemon_open(currentPath, 666);

						if(!currentDir){
							MessageBox("Failed to open directory!", MESSAGEBOX_OK);
							return 1;
						}
						
						for(int i = fileList->contents.get_length() - 1; i >= 0; i--){
							delete fileList->contents.get_at(i);//fileList->contents.remove_at(i);
						}
						fileList->contents.clear();
						
						int i = 0;
						while(lemon_readdir(currentDir, i++, &dirent)){
							fileList->contents.add_back(new ListItem(dirent.name));
						}

						fileList->ResetScrollBar();
					} else if (strcmp(dirent.name + strlen(dirent.name) - 4, ".lef")){
						lemon_spawn(dirent.name);
					}
				}
			}
		}

		PaintWindow(window);
	}

	for(;;);
}