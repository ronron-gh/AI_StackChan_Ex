#ifndef _SPIRAM_JSON_DOCUMENT_H
#define _SPIRAM_JSON_DOCUMENT_H

#include <Arduino.h>
#include <ArduinoJson.h>

// JSON DocumentをPSRAMから確保するためのAllocator
struct SpiRamAllocator {
  void* allocate(size_t size) {
//    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }

  void deallocate(void* pointer) {
    heap_caps_free(pointer);
  }

  void* reallocate(void* ptr, size_t new_size) {
    return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
  }
};

using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;


#endif //_SPIRAM_JSON_DOCUMENT_H