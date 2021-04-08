#pragma once

#include <Liballoc.h>
#include <Spinlock.h>
#include <Assert.h>

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
		assert(node);
		node = node->next;

		return *this;
	}

	ListIterator operator++(int){ // Post decrement
		ListIterator<T> v = ListIterator<T>(*this);

		assert(node);
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
		obj->next = front;

		if (!front) {
			obj->prev = obj;
			obj->next = obj;
			front = obj;
		} else {
			assert(back);

			obj->prev = back;
			back->next = obj;
			front->prev = obj;
		}

		back = obj;
		num++; // By having this being the last thing we do, when consumers take from the front, the producer should not have to acquire a lock
	}

	void add_front(const T& obj) {
		if (!back) {
			obj->prev = obj;
			obj->next = obj;
			back = obj;
		} else {
			assert(front);

			obj->next = front;
			front->prev = obj;
			back->next = obj;
		}

		front = obj;
		obj->prev = back;

		num++;
	}

	void insert(const T& obj, T& it){
		if(it == front){
			add_front(obj);
			return;
		}

		assert(it);

		obj->prev = it->prev;
		obj->next = it;

		if(it->prev){
			it->prev->next = obj;
		}
		it->prev = obj;

		num++;
	}

	T operator[](unsigned pos) {
		return get_at(pos);
	}

	T get_at(unsigned pos) {
		assert(num > 0 && pos < num && front);

		T current = front;

		for (unsigned int i = 0; i < pos && i < num && current->next; i++) current = current->next;

		return current;
	}

	__attribute__((always_inline)) inline unsigned get_length() const {
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

		current->next = current->prev = nullptr;

		if(!(--num)) front = back = nullptr;

		return current;
	}

	void remove(T obj){
		if(!front || !num){
			assert(front && num);
			return;
		}

		if (obj->next && obj->next != obj){
			assert(obj->next->prev == obj);
			
			obj->next->prev = obj->prev;
		} 
		if (obj->prev && obj->prev != obj){
			assert(obj->prev->next == obj);

			obj->prev->next = obj->next;
		}
		if (front == obj) front = obj->next;
		if (back == obj) back = obj->prev;

		obj->next = obj->prev = nullptr;

		--num;

		if(!num) front = back = nullptr;
	}

	__attribute__((always_inline)) inline T next(const T& o) const {
		if(o->next == front){
			return nullptr;
		}
		
		return o->next;
	}

	__attribute__((always_inline)) inline T get_front() const
	{
		return front;
	}

	__attribute__((always_inline)) inline T get_back() const
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

	List& operator=(const List& l){
		clear();

		for(auto& i : l){
			add_back(i);
		}

		return *this;
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

	void add_back_unlocked(const T& obj) {
		acquireLock(&lock);

		ListNode<T>* node;
		if(cache.get_length() <= 0){
			node = (ListNode<T>*)kmalloc_unlocked(sizeof(ListNode<T>));
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
		} else if (front) {
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
		
		current->next->prev = node;
		current->next = node;

		num++;

		releaseLock(&lock);
	}

	void insert(const T& obj, ListIterator<T>& it){
		if(it.node == front){
			add_front(obj);
			return;
		}

		if(it == end()){
			add_back(obj);
			return;
		}
		
		assert(it.node);
		ListNode<T>* current = it.node;

		acquireLock(&lock);

		ListNode<T>* node;
		if(!cache.get_length()){
			node = (ListNode<T>*)kmalloc(sizeof(ListNode<T>));
		} else {
			node = cache.remove_at(0);
		}

		assert(node);

		node->obj = obj;
		node->prev = current->prev;
		node->next = current;

		current->prev->next = node;
		current->prev = node;

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

	__attribute__((always_inline)) inline unsigned get_length() const {
		return num;
	}

	T remove_at(unsigned pos) {
		assert(num > 0);
		assert(pos < num);

		acquireLock(&lock);
		assert(front != nullptr);

		ListNode<T>* current = front;

		for (unsigned int i = 0; i < pos && current; i++) current = current->next;

		assert(current);

		T& obj = current->obj;

		num--;

		if (front == current) front = current->next;
		if (back == current) back = current->prev;
		if (current->next) current->next->prev = current->prev;
		if (current->prev) current->prev->next = current->next;

		if(cache.get_length() >= maxCache){
			kfree(current);
		} else {
			cache.add_back(current);
		}

		if(!num) front = back = nullptr;

		releaseLock(&lock);

		return obj;
	}

	// Used when lock on heap cannot be obtained
	T remove_at_force_cache(unsigned pos) {
		assert(num > 0);
		assert(pos < num);
		assert(front != nullptr);

		acquireLock(&lock);

		ListNode<T>* current = front;

		for (unsigned int i = 0; i < pos && current; i++) current = current->next;

		assert(current);

		T& obj = current->obj;

		num--;

		if (front == current) front = current->next;
		if (back == current) back = current->prev;
		if (current->next) current->next->prev = current->prev;
		if (current->prev) current->prev->next = current->next;

		cache.add_back(current);

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
			if (current->prev) current->prev->next = current->next;
			if (current->next) current->next->prev = current->prev;
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

		ListNode<T>* current = it.node;
		if(current){
			acquireLock(&lock);

			if (current->prev) current->prev->next = current->next;
			if (current->next) current->next->prev = current->prev;
			if (front == current) front = current->next;
			if (back == current) back = current->prev;

			it.node = current->prev;

			num--;

			if(cache.get_length() >= maxCache){
				kfree(current);
			} else {
				cache.add_back(current);
			}

			releaseLock(&lock);
		}
	}

	void allocate(unsigned count){
		acquireLock(&lock);

		while(count--){
			cache.add_back((ListNode<T>*)kmalloc(sizeof(ListNode<T>)));
		}

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