#pragma once

#include <memory.h>
#include <spin.h>
#include <assert.h>

template<typename T>
class ListNode
{
public:
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
	ListIterator& operator++(){
		if(node)
			node = node->next;

		return *this;
	}

	ListIterator& operator=(const ListIterator& other){
		node = other.node;

		return *this;
	}

	T& operator*() {
		return node->obj;
	}

	T* operator->() {
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
		return !(operator==(l, r));
	}
};

template<typename T>
class List {
public:
	volatile int lock = 0;
	
	List()
	{
		front = NULL;
		back = NULL;
		num = 0;
		lock = 0;
	}

	~List() {
		clear();
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

	void add_back(T obj) {
		ListNode<T>* node = new ListNode<T>();
		node->obj = obj;
		
		//acquireLock(&lock);

		if (!front) {
			front = node;
		}
		else if (back){
			back->next = node;
			node->prev = back;
		}
		back = node;
		num++;
		
		//releaseLock(&lock);
	}

	void add_front(T obj) {
		ListNode<T>* node = (ListNode<T>*)kmalloc(sizeof(ListNode<T>));
		*node = ListNode<T>();
		node->obj = obj;
		node->back = node->front = nullptr;

		acquireLock(&lock);

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
		assert(num > 0 && pos < num && front != nullptr);
		/*if (num <= 0 || pos >= num || front == NULL){
			T obj; // Need to do something when item not in list
			memset(&obj, 0, sizeof(T));
			return obj;
		}*/

		acquireLock(&lock);

		ListNode<T>* current = front;

		for (unsigned int i = 0; i < pos && current; i++) current = current->next;

		assert(current);
		/*if(!current){
			T t;
			memset(&t, 0, sizeof(T));
			releaseLock(&lock);
			return t;
		}*/

		T obj = current->obj;

		if (current->next) current->next->prev = current->prev;
		if (current->prev) current->prev->next = current->next;
		if (pos == 0) front = current->next;
		if (pos == --num) back = current->prev;

		releaseLock(&lock);

		kfree(current);

		return obj;
	}

	T get_front()
	{
		return front->obj;
	}

	T get_back()
	{
		return back->obj;
	}

	ListIterator<T> begin(){
		ListIterator<T> it;
		it.node = front;
		return it;
	}
	
	ListIterator<T> end(){
		ListIterator<T> it;
		it.node = nullptr;
		return it;
	}
public:
	ListNode<T>* front;
	ListNode<T>* back;
	unsigned num;
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

	void add_back(T obj) {
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

	void add_front(T obj) {
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
		assert(num > 0 && pos < num && front);
		/*if (num <= 0 || pos >= num || front == NULL){
			return nullptr;
		}*/

		T current = front;

		for (unsigned int i = 0; i < pos && current; i++) current = current->next;

		/*if(!current){
			return nullptr;
		}*/
		assert(current);

		if (current->next) current->next->prev = current->prev;
		if (current->prev) current->prev->next = current->next;
		if (pos == 0) front = current->next;
		if (pos == --num) back = current->prev;

		if(!num) front = back = nullptr;

		//current->next = current->prev = nullptr;

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
		return front->obj;
	}

	T get_back()
	{
		return back->obj;
	}
public:
	T front;
	T back;
	unsigned num;
};
