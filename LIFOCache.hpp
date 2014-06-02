#ifndef LIFOCache_HPP
#define LIFOCache_HPP

#include <string.h>
#include <iostream>
#include <cassert>
#include <stdlib.h>
#include <memory>
#include <sched.h>

using namespace std;

#ifndef bool
#define bool int
#endif
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/**
 * Threadsafe LIFO single-producer/multi-consumer cache that returns most recently posted value.
 */
template <class T> class LIFOCache {
  private: volatile long readCount;
  private: volatile long writeCount;
  private: T emptyValue;
  private: T values[2];
  private: pthread_mutex_t readerMutex;

  public: LIFOCache() { 
    this->readCount = 0;
    this->writeCount = 0;
    int rc = pthread_mutex_init(&readerMutex, NULL);
    assert(rc == 0);
  }

  public: ~LIFOCache() { 
    int rc = pthread_mutex_destroy(&readerMutex);
    assert(rc == 0);
  }

  public: T get() {
    /////////////// CRITICAL SECTION BEGIN ///////////////
    pthread_mutex_lock(&readerMutex);			//
							//
    int valueIndex = writeCount - readCount;		//
    if (valueIndex > 0) {				//
      values[0] = values[valueIndex];			//
    }							//
    readCount = writeCount;				//
    T result = values[0];				//
    pthread_mutex_unlock(&readerMutex);			//
    /////////////// CRITICAL SECTION END /////////////////
    return result;
  }

  public: void post(T value) {
    int valueIndex = writeCount - readCount + 1;
    if (valueIndex >= 2) {
      throw "LIFOCache overflow";
    }
    values[valueIndex] = value;
    writeCount++;
  }

  public: bool isFresh() { return writeCount && writeCount != readCount; }
};

template <class T> class SmartPointer {
  private: class ReferencedPointer {
    private: int references;
    private: T* ptr;

    public: inline ReferencedPointer() { 
      ptr = NULL;
      references = 0;
    }

    public: inline ReferencedPointer(T* ptr) {
      references = 1;
      this->ptr = ptr;
    }

    public: inline void decref() {
      references--;
      if (references < 0) {
	throw "ReferencedPointer extra dereference";
      }
      if (ptr && references == 0) {
	cout << "ReferencedPointer() free " << (long) ptr << endl;
	* (char *) ptr = 0; // mark as deleted
	free(ptr);
      }
    }

    public: inline void incref() { references++; }
    public: inline T* get() { return ptr; }
    public: inline int getReferences() const { return references; }
  };

  private: ReferencedPointer *pPointer;
  private: size_t length;
  private: inline void decref() { if (pPointer) { pPointer->decref(); } }
  private: inline void incref() { if (pPointer) { pPointer->incref(); } }

  /**
   * Create a smart pointer for the given data. Copied SmartPointers 
   * share the same data block, which is freed when all SmartPointers 
   * referencing that data are destroyed. Clients can provide data to 
   * be managed (i.e., count==0) or have SmartPointer allocate new memory initialized
   * from the provided data (i.e., count>0). Managed data will eventually be freed
   * by SmartPointer. Initialization data will not be freed by SmartPointer.
   *
   * @param ptr pointer to data. If ptr is null, count must be number of objects to calloc and zero-fill
   * @count number of T objects to calloc for data copied from ptr
   */
  public: inline SmartPointer(T* ptr, int count=0) {
    cout << "SmartPointer(" << (long) ptr << ")" << endl;
    this->length = count * sizeof(T);
    if (count) {
      T* pData = (T*) calloc(count, sizeof(T));
      if (ptr) {
        memcpy(pData, ptr, this->length);
      }
      this->pPointer = new ReferencedPointer(pData);
    } else {
      this->pPointer = new ReferencedPointer(ptr);
    }
  }

  public: inline SmartPointer() {
    cout << "SmartPointer(NULL)" << endl;
    this->pPointer = NULL;
    this->length = 0;
  }

  public: inline SmartPointer(const SmartPointer &that) {
    this->pPointer = that.pPointer;
    this->incref();
  }

  public: inline ~SmartPointer() { decref(); }

  public: inline SmartPointer& operator=( SmartPointer that ) {
    decref();
    this->pPointer = that.pPointer;
    incref();
    return *this;
  }

  public: inline int getReferences() { return pPointer ? pPointer->getReferences() : 0; }
  public: inline T& operator*() { throw "SmartPointer not implemented (1)"; }
  public: inline const T& operator*() const { throw "SmartPointer not implemented (2)"; }
  public: inline T* operator->() { return pPointer ? pPointer->get() : NULL; }
  public: inline const T* operator->() const { return pPointer ? pPointer->get() : NULL; }
  public: inline operator T*() const { return pPointer ? pPointer->get() : NULL; }
  public: inline size_t size() const { return length; }
};

#endif
