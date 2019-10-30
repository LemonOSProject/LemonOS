#include <serial.h>
#include <string.h>
#include <videoconsole.h>

namespace Log{

	VideoConsole* console = nullptr;

	char* logBuffer[256];

	bool buffered = false;

    void Initialize(){
		initialize_serial();
    }

	void SetVideoConsole(VideoConsole* con){
		console = con;
	}

	void EnableBuffer(){
		buffered = true;
	}

	void SetLogFile(char* path){

	}

	void Write(const char* str, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255){
		write_serial(str);
		return;
		if(console){
			//console->Print(str, r, g, b);
			console->Update();
		}
	}

    void Warning(const char* str){
		Write("\r\n");
		Write("[");
		Write("WARN", 255, 255, 0);
		Write("]    ");
		Write(str);
    }

    void Warning(unsigned long long num){
		Write("\r\n");
		Write("[");
		Write("WARN", 255, 255, 0);
		Write("]    ");
		//Write(str);
    }

    void Error(const char* str){
		Write("\r\n");
		Write("[");
		Write("ERROR", 255, 0, 0);
		Write("]   ");
		Write(str);
    }

    void Error(unsigned long long num){
		Write("\r\n");
		Write("[");
		Write("ERROR", 255, 0, 0);
		Write("]   ");
		//Write(str);
    }

    void Info(const char* str){
		Write("\r\n");
		Write("[");
		Write("INFO");
		Write("]    ");
		Write(str);
    }

    void Info(unsigned long long num, bool hex){
		Write("\r\n");
		Write("[");
		Write("INFO");
		Write("]    ");
			
		char buf[32];
		if(hex){
			buf[0] = '0';
			buf[1] = 'x';
		}
		itoa(num, (char*)(buf + (hex ? 2 : 0)), hex ? 16 : 10);
		Write(buf);
    }

}
