#pragma once

#include <stdint.h>
#include <list.h>

inline static unsigned hash(unsigned value){
	unsigned hash = value;
	
	hash = ((hash >> 5) ^ hash) * 47499631;
	hash = ((hash >> 5) ^ hash) * 47499631;
	hash = (hash >> 5) ^ hash;

	return hash;
}

inline static unsigned hash(const char* str){
	unsigned val = str[0] << 2;
	
	while(char c = *str++){
		val ^= hash(c);
	}

	return val;
}

template<typename K, typename T> // Key, Value
class HashMap{
private:
	class KeyValuePair{
		friend class HashMap;

	protected:
		T value;
		K key;

		KeyValuePair() { value = T(); key = K(); }
		KeyValuePair(K key, T value) { this->value = value; this->key = key; }
	};

	List<KeyValuePair>* buckets;
	unsigned bucketCount = 2048;

	lock_t lock = 0;
public:
	HashMap(){
		buckets = new List<KeyValuePair>[bucketCount];
		lock = 0;
	}

	HashMap(unsigned bCount){
		bucketCount = bCount;

		HashMap();
	}

	void insert(K key, const T& value){
		auto& bucket = buckets[hash(key) % bucketCount];

		acquireLock(&lock);
		bucket.add_back(KeyValuePair(key, value));
		releaseLock(&lock);
	}

	T remove(K key){
		auto& bucket = buckets[hash(key) % bucketCount];

		acquireLock(&lock);
		for(unsigned i = 0; i < bucket.get_length(); i++){
			if(bucket[i].key == key){
				auto pair = bucket.remove_at(i);
				releaseLock(&lock);
				return pair.value;
			}
		}
		releaseLock(&lock);

		return T();
	}

	T get(K key){
		auto& bucket = buckets[hash(key) % bucketCount];

		acquireLock(&lock);
		for(KeyValuePair& val : bucket){
			if(val.key == key){
				releaseLock(&lock);
				return val.value;
			}
		}
		releaseLock(&lock);

		return T();
	}

	int find(K key){
		auto& bucket = buckets[hash(key) % bucketCount];

		for(KeyValuePair& val : bucket){
			if(val.key == key){
				return 1;
			}
		}

		return 0;
	}

	~HashMap(){
		delete[](buckets);
	}
};