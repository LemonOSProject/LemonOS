#include <Lemon/Core/SHA.h> 
#include <Lemon/Core/Rotate.h>

#include <string.h>
#include <stdint.h>
#include <string.h>

#include <sstream>

SHA256::SHA256(){
	memcpy(hash, initialHash, SHA256_HASH_SIZE);
}

void SHA256::Transform(const uint8_t* data){
	uint32_t w[64];

	for(int i = 0; i < 16; i++){
		w[i] = __builtin_bswap32(reinterpret_cast<const uint32_t*>(data)[i]); // data is big endian so convert to little endian
	} // Copy chunk (512 bits/64 bytes) into first 16 bytes of w

	for(int i = 16; i < 64; i++){
		uint32_t s0 = rotateRight<uint32_t>(w[i - 15], 7) ^ rotateRight<uint32_t>(w[i - 15], 18) ^ (w[i - 15] >> 3);
		uint32_t s1 = rotateRight<uint32_t>(w[i - 2], 17) ^ rotateRight<uint32_t>(w[i - 2], 19) ^ (w[i - 2] >> 10);

		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}

	uint32_t t[8];
	for(int i = 0; i < 8; i++){
		t[i] = hash[i];
	}

	for(int i = 0; i < 64; i++){
		uint32_t s1 = rotateRight<uint32_t>(t[4], 6) ^ rotateRight<uint32_t>(t[4], 11) ^ rotateRight<uint32_t>(t[4], 25);
		uint32_t s0 = rotateRight<uint32_t>(t[0], 2) ^ rotateRight<uint32_t>(t[0], 13) ^ rotateRight<uint32_t>(t[0], 22);
		uint32_t temp1 = t[7] + s1 + SHA2_CHOOSE(t[4], t[5], t[6]) + constants[i] + w[i];
		uint32_t temp2 = s0 + SHA2_MAJOR(t[0], t[1], t[2]);

		t[7] = t[6];
		t[6] = t[5];
		t[5] = t[4];
		t[4] = t[3] + temp1;
		t[3] = t[2];
		t[2] = t[1];
		t[1] = t[0];
		t[0] = temp1 + temp2;
	}

	for(unsigned i = 0; i < SHA256_HASH_SIZE / sizeof(uint32_t); i++){
		hash[i] += t[i];
	}
}

void SHA256::Update(const void* _data, size_t count){
	const uint8_t* data = static_cast<const uint8_t*>(_data);
	memcpy(hash, initialHash, SHA256_HASH_SIZE);

	unsigned index = count;
	while(index >= SHA256_CHUNK_SIZE){
		Transform(data);

		index -= SHA256_CHUNK_SIZE;
		data += SHA256_CHUNK_SIZE;
	}

	uint8_t buffer[SHA256_CHUNK_SIZE];
	memcpy(buffer, data, index);

	buffer[index++] = 0x80; // Append a 1 on the end

	while(SHA256_CHUNK_SIZE - index != sizeof(uint64_t)){
		buffer[index++] = 0;

		if(index >= SHA256_CHUNK_SIZE){
			index = 0;
			Transform(buffer);
		}
	}

	uint64_t bigeCount = __builtin_bswap64(count * 8);
	memcpy(&buffer[index], &bigeCount, sizeof(uint64_t));
	Transform(buffer);
}

std::string SHA256::GetHash(){
	std::stringstream stream;
	for(int i = 0; i < 8; i++){
		stream << std::hex << hash[i];
	}
	return stream.str();
}