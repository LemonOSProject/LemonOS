#pragma once

#include <memory.h>
//#include <lock.h>
#define acquireLock(l)
#define releaseLock(l)

template<typename T>
class ListNode
{
public:
	ListNode* next = NULL;
	ListNode* prev = NULL;
	T obj;
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
		ListNode<T>* node = front;
		while (node->next) {
			kfree(node);
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

	void add_back(T obj) {
		acquireLock(&lock);

		ListNode<T>* node = (ListNode<T>*)kmalloc(sizeof(ListNode<T>));
		*node = ListNode<T>();
		node->obj = obj;
		
		if (!front) {
			front = node;
		}
		else {
			back->next = node;
			node->prev = back;
		}
		back = node;
		num++;
		
		releaseLock(&lock);
	}

	void add_front(T obj) {
		acquireLock(&lock);

		ListNode<T>* node = (ListNode<T>*)kmalloc(sizeof(ListNode<T>));
		*node = ListNode<T>();
		node->obj = obj;

		if (!back) {
			back = node;
		}
		else {
			front->prev = node;
			node->next = front;
		}
		front = node;
		num++;

		releaseLock(&lock);
	}

	T operator[](unsigned pos) {
		return get_at(pos);
	}

	T get_at(unsigned pos) {

		if (num <= 0 || pos >= num || front == NULL) {
			T obj; // Need to do something when item not in list
			memset(&obj, 0, sizeof(T));
			return obj;
		}

		acquireLock(&lock);

		ListNode<T>* current = front;

		for (unsigned int i = 0; i < pos && i < num && current->next; i++) current = current->next;

		releaseLock(&lock);

		return current->obj;
	}

	void replace_at(unsigned pos, T obj) {
		if (num < 0 || pos >= num) return;
		
		acquireLock(&lock);

		ListNode<T>* current = front;

		for (unsigned int i = 0; i < pos; i++) current = current->next;

		if(current)
			current->obj = obj;
		
		releaseLock(&lock);
	}

	int get_length() {
		return num;
	}

	T remove_at(unsigned pos) {
		if (num <= 0 || pos >= num){
			T t;
			return t;
		}

		acquireLock(&lock);

		ListNode<T>* current = front;

		for (unsigned int i = 0; i < pos && current; i++) current = current->next;

		if(!current){
			T t;
			releaseLock(&lock);
			return t;
		}

		T obj = current->obj;

		if (current->next) current->next->prev = current->prev;
		if (current->prev) current->prev->next = current->next;
		if (pos == 0) front = current->next;
		if (pos == --num) back = current->prev;

		kfree(current);
		releaseLock(&lock);

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
public:
	ListNode<T>* front;
	ListNode<T>* back;
	unsigned int num;
};
