#pragma once

extern "C"{
    void acquireLock(void* lock);
    void releaseLock(void* lock);
}