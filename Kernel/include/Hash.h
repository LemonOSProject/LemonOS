#pragma once

#include <stdint.h>
#include <List.h>

static inline unsigned HashU(unsigned value){
	unsigned hash = value;
	
	hash = ((hash >> 5) ^ hash) * 47499631;
	hash = ((hash >> 5) ^ hash) * 47499631;
	hash = (hash >> 5) ^ hash;

	return hash;
}

template<typename T>
unsigned Hash(const T& value);

template<typename K, typename T> // Key, Value
class HashMap{
public:
	class KeyValuePair{
		friend class HashMap;
		friend class HashMapIterator;

	protected:
		T value;
		K key;

		KeyValuePair(K key, T value) : value(value), key(key){}
	};

	class HashMapIterator {
		friend class HashMap<K, T>;
	protected:
		HashMap<K, T>* map;

		unsigned bucketIndex = 0;
		List<KeyValuePair>* bucket;

		ListIterator<KeyValuePair> listIt;
	public:
		HashMapIterator() = default;
		HashMapIterator(const HashMapIterator&) = default;

		HashMapIterator& operator++(){
			assert(listIt != bucket->end());

			listIt++;

			if(listIt == bucket->end() && bucketIndex < map->bucketCount - 1){
				bucket = map->buckets[++bucketIndex];
				listIt = bucket->begin();
			}

			return *this;
		}

		HashMapIterator& operator++(int){
			ListIterator<KeyValuePair> v = ListIterator<KeyValuePair>(*this);

			assert(listIt != bucket->end());

			listIt++;

			if(listIt == bucket->end() && bucketIndex < map->bucketCount - 1){
				bucket = map->buckets[++bucketIndex];
				listIt = bucket->begin();
			}

			return v;
		}

		T& operator*(){
			return listIt.operator*().value;
		}

		T* operator->(){
			return &listIt.operator->()->value;
		}

		friend bool operator==(const HashMapIterator& l, const HashMapIterator& r){
			if(l.listIt == r.listIt){
				return true;
			} else {
				return false;
			}
		}

		friend bool operator!=(const HashMapIterator& l, const HashMapIterator& r){
			if(l.listIt != r.listIt){
				return true;
			} else {
				return false;
			}
		}
	};

	HashMap(){
		buckets = new List<KeyValuePair>[bucketCount];
		lock = 0;
	}

	HashMap(unsigned bCount){
		bucketCount = bCount;

		buckets = new List<KeyValuePair>[bucketCount];
		lock = 0;
	}

	void insert(const K& key, const T& value){
		unsigned keyHash = Hash(key);
		auto& bucket = buckets[keyHash % bucketCount];

		acquireLock(&lock);
		for(KeyValuePair& v : bucket){
			if(v.key == key){ // Already exists, just replace
				v.value = value;

				itemCount++;

				releaseLock(&lock);
				return;
			}
		}

		bucket.add_back(KeyValuePair(key, value));
		releaseLock(&lock);
	}

	T remove(K key){
		auto& bucket = buckets[Hash(key) % bucketCount];

		acquireLock(&lock);
		for(unsigned i = 0; i < bucket.get_length(); i++){
			if(bucket[i].key == key){
				auto pair = bucket.remove_at(i);

				itemCount--;

				releaseLock(&lock);
				return pair.value;
			}
		}
		releaseLock(&lock);

		return T();
	}

	void removeValue(T value){
		acquireLock(&lock);
		for(unsigned b = 0; b < bucketCount; b++){
			auto& bucket = buckets[b];

			for(unsigned i = 0; i < bucket.get_length(); i++){
				if(bucket[i].value == value){
					bucket.remove_at(i);

					itemCount--;
					releaseLock(&lock);
					
					return;
				}
			}
		}
		releaseLock(&lock);

		return;
	}

	int get(const K& key, T& value){
		auto& bucket = buckets[Hash(key) % bucketCount];

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

	T* get(const K& key) {
		auto& bucket = buckets[Hash(key) % bucketCount];

		acquireLock(&lock);
		for(KeyValuePair& val : bucket){
			if(val.key == key){
				releaseLock(&lock);

				return &val.value;
			}
		}
		releaseLock(&lock);

		return nullptr;
	}

	int find(K key){
		auto& bucket = buckets[Hash(key) % bucketCount];

		for(KeyValuePair& val : bucket){
			if(val.key == key){
				return 1;
			}
		}

		return 0;
	}

	unsigned get_length(){
		return itemCount;
	}

	HashMapIterator begin(){
		HashMapIterator it;

		it.bucket = buckets[0];
		it.bucketIndex = 0;

		it.map = this;

		it.listIt = it.bucket->begin();

		return it;
	}

	HashMapIterator end(){
		HashMapIterator it;

		it.bucket = buckets[bucketCount - 1];
		it.bucketIndex = bucketCount - 1;

		it.map = this;

		it.listIt = it.bucket->end();

		return it;
	}

	~HashMap(){
		delete[](buckets);
	}
private:
	List<KeyValuePair>* buckets;
	unsigned bucketCount = 512;

	unsigned itemCount = 0;

	lock_t lock = 0;
};