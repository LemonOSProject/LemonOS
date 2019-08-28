#ifdef Lemon64
extern "C"{
void* kmalloc(int size){}
void* krealloc(void* p, int size){}
}
#endif 
