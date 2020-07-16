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
	class KeyValuePair{
	public:
		T value;
		K key;

		KeyValuePair() { value = T(); key = K(); }
		KeyValuePair(K key, T value) { this->value = value; this->key = key; }
	};

	List<KeyValuePair>* buckets;
	int bucketCount = 2048;

public:
	HashMap(){
		buckets = new List<KeyValuePair>[bucketCount];
	}

	void insert(K key, T& value){
		auto& bucket = buckets[hash(key) % bucketCount];

		bucket.add_back(KeyValuePair(key, value));
	}

	T remove(K key){
		auto& bucket = buckets[hash(key) % bucketCount];

		for(unsigned i = 0; i < bucket.get_length(); i++){
			if(bucket[i].key == key){
				return bucket.remove_at(i).value;
			}
		}

		return T();
	}

	T get(K key){
		auto& bucket = buckets[hash(key) % bucketCount];

		for(unsigned i = 0; i < bucket.get_length(); i++){
			KeyValuePair& val = bucket[i];

			if(val.key == key){
				return val.value;
			}
		}

		return T();
	}

	~HashMap(){
		delete[](buckets);
	}
};