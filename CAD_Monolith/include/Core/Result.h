/**
 * @file Result.h
 * @brief Error-handling foundation with no-throw guarantees
 * @author Dr. Elias Voss
 * 
 * MANDATE: Every API call MUST return Result<T, ErrorCode>.
 * No exceptions for control flow. No silent failures.
 */

#pragma once

#include <string>
#include <cstring>
#include <new>

namespace cad {

/**
 * @brief Comprehensive error codes for all system operations
 */
enum class ErrorCode : int {
    SUCCESS = 0,
    
    // Memory Errors
    MEMORY_EXHAUSTED = 1001,
    ALLOCATION_FAILED = 1002,
    MMAP_FAILED = 1003,
    
    // Geometry Errors
    GEOMETRY_DEGENERATE = 2001,
    INVALID_TOPOLOGY = 2002,
    NON_MANIFOLD_EDGE = 2003,
    ZERO_LENGTH_EDGE = 2004,
    DEGENERATE_TRIANGLE = 2005,
    TOLERANCE_VIOLATION = 2006,
    
    // Constraint Errors
    CONSTRAINT_UNSOLVABLE = 3001,
    OVER_CONSTRAINED = 3002,
    UNDER_CONSTRAINED = 3003,
    CYCLIC_DEPENDENCY = 3004,
    
    // File I/O Errors
    FILE_NOT_FOUND = 4001,
    FILE_CORRUPTED = 4002,
    CHECKSUM_MISMATCH = 4003,
    DISK_FULL = 4004,
    PERMISSION_DENIED = 4005,
    ATOMIC_RENAME_FAILED = 4006,
    
    // Threading Errors
    KERNEL_FROZEN = 5001,
    GPU_TIMEOUT = 5002,
    DEADLOCK_DETECTED = 5003,
    QUEUE_OVERFLOW = 5004,
    
    // Operation Errors
    INVALID_PARAMETER = 6001,
    OPERATION_CANCELLED = 6002,
    UNSUPPORTED_FORMAT = 6003,
    VERSION_MISMATCH = 6004,
    
    // System Errors
    INTERNAL_ERROR = 9999
};

/**
 * @brief Convert ErrorCode to human-readable string
 */
inline const char* errorCodeToString(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::SUCCESS: return "Success";
        case ErrorCode::MEMORY_EXHAUSTED: return "Memory Exhausted - Switching to Low Memory Mode";
        case ErrorCode::ALLOCATION_FAILED: return "Heap Allocation Failed";
        case ErrorCode::MMAP_FAILED: return "Memory-Mapped File Creation Failed";
        case ErrorCode::GEOMETRY_DEGENERATE: return "Degenerate Geometry Detected";
        case ErrorCode::INVALID_TOPOLOGY: return "Invalid B-Rep Topology";
        case ErrorCode::NON_MANIFOLD_EDGE: return "Non-Manifold Edge Found";
        case ErrorCode::ZERO_LENGTH_EDGE: return "Zero-Length Edge Detected";
        case ErrorCode::DEGENERATE_TRIANGLE: return "Degenerate Triangle Removed by Sanitizer";
        case ErrorCode::TOLERANCE_VIOLATION: return "Geometric Tolerance Exceeded";
        case ErrorCode::CONSTRAINT_UNSOLVABLE: return "Constraint System Has No Solution";
        case ErrorCode::OVER_CONSTRAINED: return "Sketch is Over-Constrained";
        case ErrorCode::UNDER_CONSTRAINED: return "Sketch is Under-Constrained";
        case ErrorCode::CYCLIC_DEPENDENCY: return "Cyclic Dependency in Feature Tree";
        case ErrorCode::FILE_NOT_FOUND: return "File Not Found";
        case ErrorCode::FILE_CORRUPTED: return "File Corruption Detected";
        case ErrorCode::CHECKSUM_MISMATCH: return "SHA-256 Checksum Verification Failed";
        case ErrorCode::DISK_FULL: return "Disk Space Exhausted";
        case ErrorCode::PERMISSION_DENIED: return "File Permission Denied";
        case ErrorCode::ATOMIC_RENAME_FAILED: return "Atomic File Replacement Failed";
        case ErrorCode::KERNEL_FROZEN: return "Kernel Thread Unresponsive (>500ms)";
        case ErrorCode::GPU_TIMEOUT: return "GPU Operation Timed Out";
        case ErrorCode::DEADLOCK_DETECTED: return "Potential Deadlock Detected";
        case ErrorCode::QUEUE_OVERFLOW: return "Message Queue Overflow";
        case ErrorCode::INVALID_PARAMETER: return "Invalid Parameter Provided";
        case ErrorCode::OPERATION_CANCELLED: return "Operation Cancelled by User";
        case ErrorCode::UNSUPPORTED_FORMAT: return "Unsupported File Format";
        case ErrorCode::VERSION_MISMATCH: return "File Version Incompatible";
        case ErrorCode::INTERNAL_ERROR: return "Internal System Error";
        default: return "Unknown Error Code";
    }
}

/**
 * @brief Stack trace capture for debugging (platform-specific implementation)
 */
class StackTrace {
public:
    static constexpr size_t MAX_FRAMES = 32;
    
    StackTrace() noexcept : frame_count_(0) {
        capture();
    }
    
    void capture() noexcept {
        // Platform-specific stack trace capture
        // On Linux: backtrace() from execinfo.h
        // On Windows: CaptureStackBackTrace()
        // On macOS: backtrace_symbols()
        // Implementation omitted for brevity - must be filled per platform
        frame_count_ = 0;
    }
    
    std::string toString() const noexcept {
        std::string result;
        result.reserve(512);
        result.append("Stack Trace (");
        result.append(std::to_string(frame_count_));
        result.append(" frames):\n");
        // Format stack frames here
        return result;
    }
    
private:
    void* frames_[MAX_FRAMES];
    size_t frame_count_;
};

/**
 * @brief Error context with location and stack trace
 */
struct ErrorInfo {
    ErrorCode code;
    std::string message;
    std::string file;
    int line;
    std::string function;
    StackTrace stack_trace;
    
    ErrorInfo() noexcept 
        : code(ErrorCode::SUCCESS), line(0) {}
    
    ErrorInfo(ErrorCode c, const std::string& msg = "",
              const std::string& f = "", int l = 0,
              const std::string& func = "") noexcept
        : code(c), message(msg), file(f), line(l), function(func) {
        stack_trace.capture();
    }
    
    std::string formatFullError() const noexcept {
        std::string result;
        result.reserve(256);
        result.append("[");
        result.append(file);
        result.append(":");
        result.append(std::to_string(line));
        result.append(" in ");
        result.append(function);
        result.append("] ");
        result.append(errorCodeToString(code));
        if (!message.empty()) {
            result.append(": ");
            result.append(message);
        }
        return result;
    }
};

} // namespace cad

// Debug macros for error creation
#ifdef DEBUG
    #define CAD_ERROR(code, msg) \
        cad::ErrorInfo(code, msg, __FILE__, __LINE__, __FUNCTION__)
    #define CAD_RETURN_ERROR(result) \
        return result
#else
    #define CAD_ERROR(code, msg) \
        cad::ErrorInfo(code, msg)
    #define CAD_RETURN_ERROR(result) \
        return result
#endif
