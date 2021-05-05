/*
 * freertos_cpp.cpp
 *
 *  Created on: 12.07.2020
 *      Author: marco
 */

#include "freertos/FreeRTOS.h"
#include "freertos/portable.h"

void *operator new(size_t size) { return pvPortMalloc(size); }

void *operator new[](size_t size) { return pvPortMalloc(size); }

void operator delete(void *ptr) {
    vPortFree(ptr);
    ptr = nullptr;
}

void operator delete[](void *ptr) {
    vPortFree(ptr);
    ptr = nullptr;
}

void operator delete(void *ptr, unsigned int) {
    vPortFree(ptr);
    ptr = nullptr;
}

void operator delete[](void *ptr, unsigned int) {
    vPortFree(ptr);
    ptr = nullptr;
}
