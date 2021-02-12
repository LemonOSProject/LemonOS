#pragma once

#include <memory.h>
#include <spin.h>

template<typename T>
class Vector{
	class VectorIterator {
		friend class Vector;
	protected:
		size_t pos = 0;
		const Vector<T>& vector;
	public:
		VectorIterator(const Vector<T>& newVector) : vector(newVector) {};
		VectorIterator(const VectorIterator& it) : vector(it.vector) { pos = it.pos; };

		VectorIterator& operator++(){
			pos++;
			return *this;
		}

		VectorIterator& operator=(const VectorIterator& other){
			VectorIterator(other.vector);

			pos = other.pos;

			return *this;
		}

		T& operator*() const{
			return vector.data[pos];
		}

		T* operator->() const{
			return &vector.data[pos];
		}

		friend bool operator==(const VectorIterator& l, const VectorIterator& r){
			if(l.pos == r.pos){
				return true;
			} else {
				return false;
			}
		}

		friend bool operator!=(const VectorIterator& l, const VectorIterator& r){
			if(l.pos != r.pos){
				return true;
			} else {
				return false;
			}
		}
	};
private:
	T* data = nullptr;
	size_t count = 0;
	size_t capacity = 0;
	lock_t lock = 0;
public:
	Vector() = default;

	Vector(const Vector<T>& x){
		data = new T[x.get_length()];

		for(unsigned i = 0; i < x.get_length(); i++){
			data[i] = x.data[i];
		}
	}

	T& at(size_t pos) const{
		assert(pos < count);

		return data[pos];
	}

	T& get_at(size_t pos) const{
		return at(pos);
	}

	T& operator[](size_t pos) const{
		return at(pos);
	}

	size_t size() const{
		return count;
	}

	size_t get_length() const {
		return count;
	}

	void reserve(size_t count){
		acquireLock(&lock);
		if(count > capacity){
			capacity = count + 1;
		}

		if(data){
			T* oldData = data;

			data = new T[capacity];

			for(unsigned i = 0; i < count; i++){
				data[i] = oldData[i];
			}
			
			delete[] oldData;
		} else {
			data = new T[capacity];
		}
		releaseLock(&lock);
	}

	T& add_back(T val){
		acquireLock(&lock);
		count++;

		if(count >= capacity){
			capacity += (count << 1) + 1;

			if(data){
				T* oldData = data;

				data = new T[capacity];

				for(unsigned i = 0; i < count; i++){
					data[i] = oldData[i];
				}
				
				delete[] oldData;
			} else {
				data = new T[capacity];
			}
		}

		T& ref = data[count - 1];

		ref = val;
		releaseLock(&lock);

		return ref;
	}

	T& pop_back(){
		assert(count);

		return data[--count];
	}

	void remove(const T& val){
		for(unsigned i = 0; i < count; i++){
			if(data[i] == val){
				memcpy(&data[i], &data[i + 1], count - i - 1);

				count--;
				return;
			}
		}
	}

	~Vector(){
		if(data){
			delete[] data;
		}
		data = nullptr;
	}

	VectorIterator begin() const{
		VectorIterator it = VectorIterator(*this);

		it.pos = 0;

		return it;
	}

	VectorIterator end() const{
		VectorIterator it = VectorIterator(*this);
		
		it.pos = count;

		return it;
	}

	void clear(){
		acquireLock(&lock);
		if(data){
			delete[] data;
		}

		count = capacity = 0;
		data = nullptr;
		releaseLock(&lock);
	}
};