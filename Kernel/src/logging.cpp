#include <logging.h>

#include <serial.h>
#include <string.h>
#include <videoconsole.h>
#include <liballoc.h>
#include <stdarg.h>
#include <string.h>
#include <filesystem.h>

namespace Log{

	VideoConsole* console = nullptr;

	char* logBuffer = nullptr;
	size_t logBufferPos = 0;
	size_t logBufferSize = 0;

    size_t Read(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer){
		if(size + offset > logBufferPos) size = logBufferPos - offset;
		memcpy(buffer, logBuffer + offset, size);
		return size;
	}

	fs_node_t logDevice = {
		{.name = "kernellog"},
		.flags = FS_NODE_FILE,
		
		.read = Read,
		.write = nullptr,
		.open = nullptr,
		.close = nullptr,
		.readDir = nullptr,
		.findDir = nullptr,
	};

    void Initialize(){
		initialize_serial();
    }

	void SetVideoConsole(VideoConsole* con){
		console = con;
	}

	void EnableBuffer(){
		logBufferSize = 4096;
		logBuffer = (char*)kmalloc(logBufferSize);
		logBufferPos = 0;

		fs::RegisterDevice(&logDevice);
	}

	void WriteN(const char* str, int n){
		write_serial_n(str, n);

		if(logBuffer){
			if(n + logBufferPos > logBufferSize){
				logBufferSize += 4096;
				char* oldBuf = logBuffer;
				logBuffer = (char*)kmalloc(logBufferSize);
				memcpy(logBuffer, oldBuf, logBufferPos);
				kfree(oldBuf);
			}

			memcpy(logBuffer + logBufferPos, str, n);
			logBufferPos += n;

			logDevice.size = logBufferPos;
		}
		
		if(console){
			console->PrintN(str, n, 255, 255, 255);
			console->Update();
		}
	}

	void Write(const char* str, uint8_t r, uint8_t g, uint8_t b){
		WriteN(str, strlen(str));
	}

	void Write(unsigned long long num, bool hex, uint8_t r, uint8_t g, uint8_t b){
		char buf[32];
		if(hex){
			buf[0] = '0';
			buf[1] = 'x';
		}
		itoa(num, (char*)(buf + (hex ? 2 : 0)), hex ? 16 : 10);
		WriteN(buf, strlen(buf));
	}

	void WriteF(const char* __restrict format, va_list args){
	
		while (*format != '\0') {
			if (format[0] != '%' || format[1] == '%') {
				if (format[0] == '%')
					format++;
				size_t amount = 1;
				while (format[amount] && format[amount] != '%')
					amount++;
				WriteN(format, amount);
				format += amount;
				continue;
			}
	
			const char* format_begun_at = format++;

			bool hex = true;
			switch(*format){
				case 'c': {
					format++;
					auto arg = (char) va_arg(args, int /* char promotes to int */);
					WriteN(&arg, 1);
					break;
				} case 's': {
					format++;
					auto arg = va_arg(args, const char*);
					size_t len = strlen(arg);
					WriteN(arg, len);
					break;
				} case 'd': {
					hex = false;
				} case 'x': {
					format++;
					auto arg = va_arg(args, unsigned long long);
					Write(arg, hex);
					break;
				} default:
					format = format_begun_at;
					size_t len = strlen(format);
					WriteN(format, len);
					format += len;
			}
		}
	}

	void Warning(const char* __restrict fmt, ...){
		Write("\r\n");
		Write("[");
		Write("WARN", 255, 255, 0);
		Write("]    ");
		va_list args;
		va_start(args, fmt);
		WriteF(fmt, args);
		va_end(args);
    }

    void Error(const char* __restrict fmt, ...){
		Write("\r\n");
		Write("[");
		Write("ERROR", 255, 0, 0);
		Write("]   ");
		va_list args;
		va_start(args, fmt);
		WriteF(fmt, args);
		va_end(args);
    }

    void Info(const char* __restrict fmt, ...){
		Write("\r\n");
		Write("[");
		Write("INFO");
		Write("]    ");
		va_list args;
		va_start(args, fmt);
		WriteF(fmt, args);
		va_end(args);
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

    void Error(unsigned long long num, bool hex){
		Write("\r\n");
		Write("[");
		Write("ERROR", 255, 0, 0);
		Write("]   ");char buf[32];
		if(hex){
			buf[0] = '0';
			buf[1] = 'x';
		}
		itoa(num, (char*)(buf + (hex ? 2 : 0)), hex ? 16 : 10);
		Write(buf);
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
