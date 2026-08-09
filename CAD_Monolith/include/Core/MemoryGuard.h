/**
 * @file MemoryGuard.h
 * @brief RAII memory allocation with low-memory mode fallback
 * @author Dr. Elias Voss
 * 
 * MANDATE: All heap allocations MUST be wrapped in guard classes.
 * On std::bad_alloc: log stack trace, flush caches, switch to mmap.
 */

#pragma once

#include "Core/Result.h"
#include <memory>
#include <atomic>
#include <cstring>
#include <iostream>
#include <fstream>

namespace cad {

/**
 * @brief Global memory state tracker
 */
class MemorySystem {
public:
    static MemorySystem& instance() noexcept {
        static MemorySystem inst;
        return inst;
    }
    
    bool isLowMemoryMode() const noexcept {
        return low_memory_mode_.load(std::memory_order_relaxed);
    }
    
    void enableLowMemoryMode() noexcept {
        if (!low_memory_mode_.exchange(true, std::memory_order_relaxed)) {
            std::cerr << "[MEMORY] Entering Low Memory Mode - Flushing caches...\n";
            flushAllCaches();
        }
    }
    
    void registerCache(void* cache_ptr, size_t size, void (*flush_func)(void*)) noexcept {
        // In production: maintain list of flushable caches
        (void)cache_ptr; (void)size; (void)flush_func;
    }
    
private:
    MemorySystem() noexcept : low_memory_mode_(false) {}
    
    void flushAllCaches() noexcept {
        // Flush render caches, undo history beyond current transaction, etc.
    }
    
    std::atomic<bool> low_memory_mode_;
};

/**
 * @brief Custom allocator with bad_alloc handling
 */
template<typename T>
class SafeAllocator {
public:
    using value_type = T;
    
    SafeAllocator() noexcept = default;
    
    template<typename U>
    SafeAllocator(const SafeAllocator<U>&) noexcept {}
    
    T* allocate(std::size_t n) {
        if (n == 0) return nullptr;
        
        try {
            return static_cast<T*>(::operator new(n * sizeof(T)));
        } catch (const std::bad_alloc& e) {
            MemorySystem::instance().enableLowMemoryMode();
            
            // Attempt disk-backed allocation
            void* mmap_ptr = allocateViaMMap(n * sizeof(T));
            if (mmap_ptr) {
                return static_cast<T*>(mmap_ptr);
            }
            
            // Last resort: return nullptr and let caller handle via Result
            return nullptr;
        }
    }
    
    void deallocate(T* ptr, std::size_t n) noexcept {
        if (ptr) {
            ::operator delete(ptr);
            (void)n;
        }
    }
    
private:
    void* allocateViaMMap(size_t bytes) noexcept {
        #ifdef _WIN32
            // Windows: CreateFileMapping + MapViewOfFile
            return nullptr; // Placeholder
        #else
            // POSIX: mmap with MAP_ANONYMOUS | MAP_PRIVATE
            return nullptr; // Placeholder
        #endif
    }
};

/**
 * @brief RAII wrapper for heap allocations with automatic error logging
 */
template<typename T>
class HeapGuard {
public:
    template<typename... Args>
    explicit HeapGuard(Args&&... args) 
        : ptr_(nullptr), allocated_(false) {
        
        try {
            ptr_ = new T(std::forward<Args>(args)...);
            allocated_ = true;
        } catch (const std::bad_alloc& e) {
            std::cerr << "[HEAPGUARD] Allocation failed for type: " 
                      << typeid(T).name() << "\n";
            std::cerr << "[HEAPGUARD] Stack trace:\n";
            StackTrace st;
            st.capture();
            std::cerr << st.toString();
            
            MemorySystem::instance().enableLowMemoryMode();
            // ptr_ remains nullptr, allocated_ remains false
        }
    }
    
    ~HeapGuard() noexcept {
        if (allocated_ && ptr_) {
            try {
                delete ptr_;
            } catch (...) {
                // No-throw guarantee: swallow exceptions in destructor
                std::cerr << "[HEAPGUARD] Exception in destructor suppressed\n";
            }
        }
    }
    
    // Move semantics with no-throw guarantee
    HeapGuard(HeapGuard&& other) noexcept 
        : ptr_(other.ptr_), allocated_(other.allocated_) {
        other.ptr_ = nullptr;
        other.allocated_ = false;
    }
    
    HeapGuard& operator=(HeapGuard&& other) noexcept {
        if (this != &other) {
            if (allocated_ && ptr_) {
                try { delete ptr_; } catch (...) {}
            }
            ptr_ = other.ptr_;
            allocated_ = other.allocated_;
            other.ptr_ = nullptr;
            other.allocated_ = false;
        }
        return *this;
    }
    
    // Delete copy operations (unique ownership)
    HeapGuard(const HeapGuard&) = delete;
    HeapGuard& operator=(const HeapGuard&) = delete;
    
    T* get() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
    
    explicit operator bool() const noexcept { return allocated_ && ptr_; }
    
    bool isValid() const noexcept { return allocated_ && ptr_; }
    
    Result<T*, ErrorCode> getResult() const noexcept {
        if (allocated_ && ptr_) {
            return Result<T*, ErrorCode>::ok(ptr_);
        } else {
            return Result<T*, ErrorCode>::err(ErrorCode::ALLOCATION_FAILED);
        }
    }
    
private:
    T* ptr_;
    bool allocated_;
};

/**
 * @brief Smart pointer with allocation failure handling
 */
template<typename T, typename... Args>
Result<std::unique_ptr<T>, ErrorCode> makeSafe(Args&&... args) noexcept {
    try {
        auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
        return Result<std::unique_ptr<T>, ErrorCode>::ok(std::move(ptr));
    } catch (const std::bad_alloc& e) {
        std::cerr << "[makeSafe] Allocation failed: " << e.what() << "\n";
        MemorySystem::instance().enableLowMemoryMode();
        return Result<std::unique_ptr<T>, ErrorCode>::err(ErrorCode::MEMORY_EXHAUSTED);
    }
}

} // namespace cad
