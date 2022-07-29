#pragma once

#include <mutex>
#include <unordered_map>

namespace RefCounted {

class Object {
public:
    void* ptr;
    int ref;

    Object(void* obj) {
        ptr = obj;
        ref = 1;
    }
    
    void add_ref() {
        ref++;
    }

    int release_ref() {
        return --ref;
    }
    
    template<class T>
    void destroy() {
        delete static_cast<T*>(ptr);
    }
};

static std::unordered_map<void *, Object> object_pool;
static std::mutex global_object_lock;

void new_ref_counted(void* ptr) {
    std::lock_guard<std::mutex> lg(global_object_lock);
    object_pool.emplace(ptr, Object(ptr));
}

void add_ref(void* ptr) {
    std::lock_guard<std::mutex> lg(global_object_lock);
    object_pool.at(ptr).add_ref();
}

template<class T>
void release(T* ptr) {
    std::lock_guard<std::mutex> lg(global_object_lock);
    if (object_pool.at(ptr).release_ref() == 0) {
        object_pool.at(ptr).template destroy<T>();
        object_pool.erase(ptr);
    }
}

} // namespace RefCounted
