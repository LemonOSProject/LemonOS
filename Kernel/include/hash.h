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
	unsigned bucketCount = 512;

	lock_t lock = 0;
public:
	HashMap(){
		buckets = new List<KeyValuePair>[bucketCount];
		lock = 0;
	}

	HashMap(unsigned bCount){
		bucketCount = bCount;

		buckets = new List<KeyValuePair>[bucketCount];
		lock = 0;
	}

	void insert(K key, const T& value){
		auto& bucket = buckets[hash(key) % bucketCount];

		acquireLock(&lock);
		for(KeyValuePair& v : bucket){
			if(v.key == key){ // Already exists, just replace
				v.value = value;
				releaseLock(&lock);

				return;
			}
		}

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

	int get(const K& key, T& value){
		auto& bucket = buckets[hash(key) % bucketCount];

		acquireLock(&lock);
		for(KeyValuePair& val : bucket){
			if(val.key == key){
				releaseLock(&lock);

				value = val.value;
				return 1;
			}
		}
		releaseLock(&lock);

		return 0;
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