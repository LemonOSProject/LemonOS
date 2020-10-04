#pragma once

#include <memory.h>
#include <spin.h>
#include <assert.h>

template<typename T>
struct ListNode
{
	ListNode* next = nullptr;
	ListNode* prev = nullptr;
	T obj;
};

template<typename T>
class List;

template<typename T>
class ListIterator {
	friend class List<T>;
protected:
	ListNode<T>* node = nullptr;
public:
	ListIterator() = default;
	ListIterator(const ListIterator<T>&) = default;

	ListIterator& operator++(){
		if(node)
			node = node->next;

		return *this;
	}

	ListIterator operator++(int){ // Post decrement
		ListIterator<T> v = ListIterator<T>(*this);

		if(node)
			node = node->next;

		return v;
	}

	ListIterator& operator=(const ListIterator& other){
		node = other.node;

		return *this;
	}

	T& operator*() {
		assert(node);

		return node->obj;
	}

	T* operator->() {
		assert(node);

		return &node->obj;
	}

	friend bool operator==(const ListIterator& l, const ListIterator& r){
		if(l.node == r.node){
			return true;
		} else {
			return false;
		}
	}

	friend bool operator!=(const ListIterator& l, const ListIterator& r){
		if(l.node != r.node){
			return true;
		} else {
			return false;
		}
	}
};

// Type is required to be a pointer with the members next and prev
template<typename T>
class FastList {
public:
	FastList()
	{
		front = NULL;
		back = NULL;
		num = 0;
	}

	~FastList() {
	}

	void clear() {
		front = NULL;
		back = NULL;
		num = 0;
	}

	void add_back(const T& obj) {
		if (!front) {
			front = obj;
			obj->prev = obj;
		} else if (back) {
			back->next = obj;
			obj->prev = back;
		}
		back = obj;
		obj->next = front;//obj->next = nullptr;
		num++;
	}

	void add_front(const T& obj) {
		if (!back) {
			back = obj;
		}
		else if(front) {
			front->prev = obj;
			obj->next = front;
		}
		front = obj;
		obj->prev = back;
		num++;
	}

	T operator[](unsigned pos) {
		return get_at(pos);
	}

	T get_at(unsigned pos) {
		assert(num > 0 && pos < num && front);
		/*if (num <= 0 || pos >= num || front == NULL) {
			return nullptr;
		}*/

		T current = front;

		for (unsigned int i = 0; i < pos && i < num && current->next; i++) current = current->next;

		return current;
	}

	unsigned get_length() {
		return num;
	}

	T remove_at(unsigned pos) {
		assert(num > 0);
		assert(pos < num);
		assert(front != nullptr);

		T current = front;

		for (unsigned int i = 0; i < pos && current; i++) current = current->next;

		assert(current);

		if (current->next) current->next->prev = current->prev;
		if (current->prev) current->prev->next = current->next;
		if (front == current) front = current->next;
		if (back == current) back = current->prev;

		if(!(--num)) front = back = nullptr;

		return current;
	}

	void remove(T obj){
		if (obj->next) obj->next->prev = obj->prev;
		if (obj->prev) obj->prev->next = obj->next;
		if (front == obj) front = obj->next;
		if (back == obj) back = obj->prev;

		--num;

		if(!num) front = back = nullptr;
	}

	T get_front()
	{
		return front;
	}

	T get_back()
	{
		return back;
	}
public:
	T front;
	T back;
	unsigned num;
};

template<typename T>
class List {
private:
	ListNode<T>* front;
	ListNode<T>* back;
	unsigned num;
	volatile int lock = 0;

	const unsigned maxCache = 6;
	FastList<ListNode<T>*> cache; // Prevent allocations by caching ListNodes
public:
	
	List()
	{
		front = NULL;
		back = NULL;
		num = 0;
		lock = 0;
	}

	~List() {
		releaseLock(&lock);

		clear();

		while(cache.get_length()){
			kfree(cache.remove_at(0));
		}
	}

	void clear() {
		acquireLock(&lock);

		ListNode<T>* node = front;
		while (node && node->next) {
			ListNode<T>* n = node->next;
			kfree(node);
			node = n;
		}
		front = NULL;
		back = NULL;
		num = 0;

		releaseLock(&lock);
	}

	void add_back(const T& obj) {
		acquireLock(&lock);

		ListNode<T>* node;
		if(cache.get_length() <= 0){
			node = (ListNode<T>*)kmalloc(sizeof(ListNode<T>));
		} else {
			node = cache.remove_at(0);
		}
		
		assert(node);

		node->obj = obj;
		node->next = node->prev = nullptr;

		if (!front) {
			front = node;
		}
		else if (back){
			back->next = node;
			node->prev = back;
		}
		back = node;
		num++;
		
		releaseLock(&lock);
	}

	void add_front(const T& obj) {
		acquireLock(&lock);

		ListNode<T>* node;
		if(!cache.get_length()){
			node = (ListNode<T>*)kmalloc(sizeof(ListNode<T>));
		} else {
			node = cache.remove_at(0);
		}

		node->obj = obj;
		node->next = node->prev = nullptr;

		if (!back) {
			back = node;
		}
		else if (front) {
			front->prev = node;
			node->next = front;
		}
		front = node;
		num++;

		releaseLock(&lock);
	}

	void insert(const T& obj, size_t pos){
		if(!num){
			add_back(obj);
			return;
		}

		if(!pos){
			add_front(obj);
			return;
		}

		acquireLock(&lock);
		ListNode<T>* current = front;

		for (unsigned int i = 0; i < pos && i < num && current->next; i++) current = current->next;

		ListNode<T>* node;
		if(!cache.get_length()){
			node = (ListNode<T>*)kmalloc(sizeof(ListNode<T>));
		} else {
			node = cache.remove_at(0);
		}

		assert(node);

		node->obj = obj;
		node->prev = current;
		node->next = current->next;
		current->next = node;

		num++;

		releaseLock(&lock);
	}

	T& operator[](unsigned pos) {
		return get_at(pos);
	}

	T& get_at(unsigned pos) {
		assert(num > 0 && pos < num && front != nullptr);

		ListNode<T>* current = front;

		//acquireLock(&lock);

		for (unsigned int i = 0; i < pos && i < num && current->next; i++) current = current->next;

		//releaseLock(&lock);

		return current->obj;
	}

	void replace_at(unsigned pos, T obj) {
		assert(num > 0 && pos < num);
		//if (num < 0 || pos >= num) return;
		
		acquireLock(&lock);

		ListNode<T>* current = front;

		for (unsigned int i = 0; i < pos; i++) current = current->next;

		if(current)
			current->obj = obj;
		
		releaseLock(&lock);
	}

	unsigned get_length() {
		return num;
	}

	T remove_at(unsigned pos) {
		assert(num > 0);
		assert(pos < num);
		assert(front != nullptr);

		acquireLock(&lock);

		ListNode<T>* current = front;

		for (unsigned int i = 0; i < pos && current; i++) current = current->next;

		assert(current);

		T obj = current->obj;

		num--;

		if (current->next) current->next->prev = current->prev;
		if (current->prev) current->prev->next = current->next;
		if (pos == 0) front = current->next;
		if (pos == num) back = current->prev;

		if(cache.get_length() >= maxCache){
			kfree(current);
		} else {
			cache.add_back(current);
		}

		if(!num) front = back = nullptr;

		releaseLock(&lock);

		return obj;
	}

	void remove(T val){
		if(num <= 0 || !front){
			return;
		}

		acquireLock(&lock);

		ListNode<T>* current = front;

		while(current && current != back && current->obj != val) current = current->next;

		if(current){
			current->prev->next = current->next;
			current->next->prev = current->prev;
			if (front == current) front = current->next;
			if (back == current) back = current->prev;

			num--;

			if(cache.get_length() >= maxCache){
				kfree(current);
			} else {
				cache.add_back(current);
			}
		}

		releaseLock(&lock);
	}


	void remove(ListIterator<T>& it){
		assert(it.node);

		acquireLock(&lock);

		it.node->prev->next = it.node->next;
		it.node->next->prev = it.node->prev;

		if (front == it.node) front = it.node->next;
		if (back == it.node) back = it.node->prev;

		num--;

		if(cache.get_length() >= maxCache){
			kfree(it.node);
		} else {
			cache.add_back(it.node);
		}

		it.node = nullptr;

		releaseLock(&lock);
	}

	T& get_front() const {
		assert(front);
		return front->obj;
	}

	T& get_back() const {
		assert(back);
		return back->obj;
	}

	ListIterator<T> begin() const {
		ListIterator<T> it;

		if(!num || !front){
			it.node = nullptr;
		} else {
			it.node = front;
		}

		return it;
	}
	
	ListIterator<T> end() const {
		ListIterator<T> it;
		it.node = nullptr;
		return it;
	}
};