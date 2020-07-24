#pragma once

#include <memory.h>
#include <spin.h>

template<typename T>
class Vector{
	class VectorIterator {
		friend class Vector;
	protected:
		size_t pos = 0;
		Vector<T>& vector;
	public:
		VectorIterator(Vector<T>& newVector) : vector(newVector) {
		}

		VectorIterator& operator++(){
			pos++;
			return *this;
		}

		VectorIterator& operator=(const VectorIterator& other){
			VectorIterator(other.vector);

			pos = other.pos;

			return *this;
		}

		T& operator*(){
			return vector.data[pos];
		}

		T* operator->(){
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
public:
	T& at(size_t pos){
		assert(pos < count);

		return data[pos];
	}

	T& get_at(size_t pos){
		return at(pos);
	}

	T& operator[](size_t pos){
		return at(pos);
	}

	size_t size(){
		return count;
	}

	size_t get_length(){
		return count;
	}

	void reserve(size_t count){
		if(count > capacity){
			capacity = count + 1;
		}

		if(data){
			T* oldData = data;

			data = new T[capacity];
			memcpy(data, oldData, count * sizeof(T));
			
			delete oldData;
		} else {
			data = new T[capacity];
		}
	}

	void add_back(T val){
		count++;

		if(count >= capacity){
			capacity += (count >> 1) + 1;

			if(data){
				T* oldData = data;

				data = new T[capacity];
				memcpy(data, oldData, count * sizeof(T));
				
				delete oldData;
			} else {
				data = new T[capacity];
			}
		}

		data[count - 1] = val;
	}

	T& pop_back(){
		assert(count);

		return data[--count];
	}

	~Vector(){
		if(data){
			delete data;
		}
	}

	VectorIterator begin(){
		VectorIterator it = VectorIterator(*this);

		it.pos = 0;

		return it;
	}

	VectorIterator end(){
		VectorIterator it = VectorIterator(*this);
		
		it.pos = count;

		return it;
	}

	void clear(){
		if(data){
			delete data;
		}

		count = capacity = 0;
		data = nullptr;
	}
};