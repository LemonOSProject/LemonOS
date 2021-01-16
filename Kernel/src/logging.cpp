#include <logging.h>

#include <serial.h>
#include <string.h>
#include <videoconsole.h>
#include <liballoc.h>
#include <stdarg.h>
#include <string.h>
#include <fs/filesystem.h>
#include <pty.h>
#include <device.h>

namespace Log{

	VideoConsole* console = nullptr;

	bool logBufferEnabled = false;
	char* logBuffer = nullptr;
	size_t logBufferPos = 0;
	size_t logBufferSize = 0;
	size_t logBufferMaxSize = 0x100000; // 1MB

	int logLock = 0;
	
	void WriteN(const char* str, size_t n);

	class LogDevice : public Device{
	public:
		LogDevice(char* name) : Device(name, TypeGenericDevice){
			flags = FS_NODE_FILE;
		}

		ssize_t Read(size_t offset, size_t size, uint8_t *buffer){
			if(!logBuffer) return 0;
			if(offset > logBufferPos) return 0;

			if(size + offset > logBufferPos) size = logBufferPos - offset;
			memcpy(buffer, logBuffer + offset, size);

			return size;
		}

		ssize_t Write(size_t offset, size_t size, uint8_t *buffer){
			WriteN((char*)buffer, size);

			return size;
		}

		int Ioctl(uint64_t cmd, uint64_t arg){
			if(cmd == TIOCGWINSZ) return 0; // Pretend to be a terminal
			else return -1;
		}
	};

	LogDevice* logDevice;

    void Initialize(){
		initialize_serial();

		logDevice = new LogDevice("kernellog");
    }

	void LateInitialize(){
		DeviceManager::RegisterDevice(*logDevice);
	}

	void SetVideoConsole(VideoConsole* con){
		console = con;
	}

	void EnableBuffer(){
		if(!logBuffer){
			logBufferSize = 4096;
			logBuffer = (char*)kmalloc(logBufferSize);
			logBufferPos = 0;
		}

		logBufferEnabled = true;
	}

	void DisableBuffer(){
		logBufferEnabled = false;
	}

	void WriteN(const char* str, size_t n){
		write_serial_n(str, n);
		
		if(console){
			console->PrintN(str, n, 255, 255, 255);
		}

		if(logBufferEnabled){
			if(n >= logBufferMaxSize){
				n -= (n - logBufferMaxSize);
			}

			if(n + logBufferPos > logBufferMaxSize){
				size_t discard = (n + logBufferPos) - logBufferMaxSize; // Amount of bytes to discard

				logBufferPos -= discard;
				memcpy(logBuffer, logBuffer + discard, logBufferPos); 
			}

			if(n + logBufferPos > logBufferSize){
				logBufferSize += 4096;
				char* oldBuf = logBuffer;
				logBuffer = (char*)kmalloc(logBufferSize);
				memcpy(logBuffer, oldBuf, logBufferPos);
				kfree(oldBuf);
			}

			memcpy(logBuffer + logBufferPos, str, n);
			logBufferPos += n;

			logDevice->size = logBufferPos;
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
				} case 'Y': {
					format++;
					auto arg = (bool)va_arg(args, unsigned int);
					Write(arg ? "yes" : "no");
					break;
				} case 's': {
					format++;
					auto arg = va_arg(args, const char*);
					size_t len = strlen(arg);
					WriteN(arg, len);
					break;
				} case 'd': 
				  case 'i': {
					format++;
					long arg = va_arg(args, long);
					if(arg < 0){
						WriteN("-", 1);
						Write(-arg, false);
					} else {
						Write(arg, false);
					}
					break;
				} case 'u': {
					hex = false;
					[[fallthrough]];
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

		if(console) 
			console->Update();
	}

    void Print(const char* __restrict fmt, ...){
		va_list args;
		va_start(args, fmt);
		WriteF(fmt, args);
		va_end(args);
    }

	void Warning(const char* __restrict fmt, ...){
		//acquireLock(&logLock);
		Write("\r\n[WARN]    ", 255, 255, 0);
		va_list args;
		va_start(args, fmt);
		WriteF(fmt, args);
		va_end(args);
		//releaseLock(&logLock);
    }

    void Error(const char* __restrict fmt, ...){
		unlockSerial();
		Write("\r\n[ERROR]   ", 255, 0, 0);
		va_list args;
		va_start(args, fmt);
		WriteF(fmt, args);
		va_end(args);
    }

    void Info(const char* __restrict fmt, ...){
		//acquireLock(&logLock);
		Write("\r\n[INFO]    ");
		va_list args;
		va_start(args, fmt);
		WriteF(fmt, args);
		va_end(args);
		//releaseLock(&logLock);
    }

    void Warning(const char* str){
		Write("\r\n[WARN]    ", 255, 255, 0);
		Write(str);
    }

    void Warning(unsigned long long num){
		Write("\r\n[WARN]    ", 255, 255, 0);
		//Write(str);
    }

    void Error(const char* str){
		Write("\r\n[ERROR]   ", 255, 0, 0);
		Write(str);
    }

    void Error(unsigned long long num, bool hex){
		Write("\r\n[ERROR]   ", 255, 0, 0);
		char buf[32];
		
		if(hex){
			buf[0] = '0';
			buf[1] = 'x';
		}
		itoa(num, (char*)(buf + (hex ? 2 : 0)), hex ? 16 : 10);
		Write(buf);
    }

    void Info(const char* str){
		Write("\r\n[INFO]    ");
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
