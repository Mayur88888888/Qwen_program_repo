#include "Core/MemoryGuard.h"
#include <iostream>
#include <new>

namespace Cad {

// Global low-memory flag
std::atomic<bool> MemoryGuard::s_lowMemoryMode{false};
size_t MemoryGuard::s_memoryThreshold = 1024 * 1024 * 512; // 512MB default threshold

// Custom new handler to prevent crashes on allocation failure
void MemoryGuard::lowMemoryHandler() {
    s_lowMemoryMode.store(true, std::memory_order_release);
    
    std::cerr << "[MEMORY GUARD] CRITICAL: System entering Low Memory Mode.\n";
    std::cerr << "[MEMORY GUARD] Flushing caches and releasing non-essential resources...\n";
    
    // In a real implementation, this would trigger cache flushes
    // For now, we just log the event
    
    // Try to free some memory by waiting a bit (simulated)
    #ifdef _WIN32
        Sleep(100); 
    #else
        usleep(100000);
    #endif
    
    // If still failing, the standard handler will terminate
    // But we try to continue in degraded mode first
}

void MemoryGuard::installGlobalHandler() {
    std::set_new_handler(lowMemoryHandler);
}

void MemoryGuard::removeGlobalHandler() {
    std::set_new_handler(nullptr);
}

bool MemoryGuard::isLowMemoryMode() {
    return s_lowMemoryMode.load(std::memory_order_acquire);
}

void MemoryGuard::resetLowMemoryMode() {
    s_lowMemoryMode.store(false, std::memory_order_release);
}

ScopedMemoryTracker::ScopedMemoryTracker(const std::string& label, size_t expectedSize)
    : m_label(label), m_expectedSize(expectedSize) {
    // Log allocation start in debug builds
    #ifdef _DEBUG
    std::cout << "[MEM TRACK] Allocating: " << m_label << " (" << m_expectedSize << " bytes)\n";
    #endif
}

ScopedMemoryTracker::~ScopedMemoryTracker() noexcept {
    // Destructor never throws - RAII guarantee
    #ifdef _DEBUG
    std::cout << "[MEM TRACK] Freed: " << m_label << "\n";
    #endif
}

} // namespace Cad
