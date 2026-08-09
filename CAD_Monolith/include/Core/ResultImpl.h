/**
 * @file ResultImpl.h
 * @brief Complete implementation of Result<T, ErrorCode> with no-throw guarantees
 * @author Dr. Elias Voss
 */

#pragma once

#include "Core/Result.h"
#include <utility>
#include <new>

namespace cad {

/**
 * @brief Monadic result type for error handling without exceptions
 * 
 * Usage:
 *   Result<int, ErrorCode> divide(int a, int b) {
 *       if (b == 0) return Result<int, ErrorCode>::err(ErrorCode::INVALID_PARAMETER);
 *       return Result<int, ErrorCode>::ok(a / b);
 *   }
 */
template<typename T>
class Result<T, ErrorCode> {
public:
    /**
     * @brief Construct a successful result
     */
    static Result ok(T value) noexcept {
        Result r;
        r.is_ok_ = true;
        new (&r.storage_) T(std::move(value));
        return r;
    }
    
    /**
     * @brief Construct an error result
     */
    static Result err(ErrorCode code, const std::string& message = "") noexcept {
        Result r;
        r.is_ok_ = false;
        r.error_ = ErrorInfo(code, message);
        return r;
    }
    
    /**
     * @brief Default constructor (error state)
     */
    Result() noexcept : is_ok_(false) {}
    
    /**
     * @brief Destructor - no-throw guarantee
     */
    ~Result() noexcept {
        if (is_ok_ && has_value_) {
            try {
                reinterpret_cast<T*>(&storage_)->~T();
            } catch (...) {
                // Suppress all exceptions in destructor
            }
        }
    }
    
    /**
     * @brief Copy constructor
     */
    Result(const Result& other) noexcept : is_ok_(other.is_ok_), has_value_(other.has_value_) {
        if (is_ok_ && has_value_) {
            new (&storage_) T(*reinterpret_cast<const T*>(&other.storage_));
        } else {
            error_ = other.error_;
        }
    }
    
    /**
     * @brief Move constructor
     */
    Result(Result&& other) noexcept : is_ok_(other.is_ok_), has_value_(other.has_value_) {
        if (is_ok_ && has_value_) {
            new (&storage_) T(std::move(*reinterpret_cast<T*>(&other.storage_)));
        } else {
            error_ = std::move(other.error_);
        }
        other.has_value_ = false;
    }
    
    /**
     * @brief Copy assignment
     */
    Result& operator=(const Result& other) noexcept {
        if (this != &other) {
            destroy();
            is_ok_ = other.is_ok_;
            has_value_ = other.has_value_;
            if (is_ok_ && has_value_) {
                new (&storage_) T(*reinterpret_cast<const T*>(&other.storage_));
            } else {
                error_ = other.error_;
            }
        }
        return *this;
    }
    
    /**
     * @brief Move assignment
     */
    Result& operator=(Result&& other) noexcept {
        if (this != &other) {
            destroy();
            is_ok_ = other.is_ok_;
            has_value_ = other.has_value_;
            if (is_ok_ && has_value_) {
                new (&storage_) T(std::move(*reinterpret_cast<T*>(&other.storage_)));
            } else {
                error_ = std::move(other.error_);
            }
            other.has_value_ = false;
        }
        return *this;
    }
    
    /**
     * @brief Check if result is successful
     */
    bool isSuccess() const noexcept { return is_ok_; }
    bool isError() const noexcept { return !is_ok_; }
    explicit operator bool() const noexcept { return is_ok_; }
    
    /**
     * @brief Access the value (undefined behavior if error state)
     */
    T& value() noexcept {
        return *reinterpret_cast<T*>(&storage_);
    }
    
    const T& value() const noexcept {
        return *reinterpret_cast<const T*>(&storage_);
    }
    
    /**
     * @brief Access the error (undefined behavior if success state)
     */
    ErrorCode errorCode() const noexcept {
        return error_.code;
    }
    
    const ErrorInfo& errorInfo() const noexcept {
        return error_;
    }
    
    std::string errorMessage() const noexcept {
        return error_.formatFullError();
    }
    
    /**
     * @brief Get value or default
     */
    T valueOr(T default_value) const noexcept {
        if (is_ok_ && has_value_) {
            return *reinterpret_cast<const T*>(&storage_);
        }
        return default_value;
    }
    
    /**
     * @brief Transform success value
     */
    template<typename F>
    auto map(F&& func) const noexcept -> Result<decltype(func(std::declval<T>())), ErrorCode> {
        using ReturnType = decltype(func(std::declval<T>()));
        if (is_ok_ && has_value_) {
            return Result<ReturnType, ErrorCode>::ok(func(*reinterpret_cast<const T*>(&storage_)));
        } else {
            return Result<ReturnType, ErrorCode>::err(error_.code, error_.message);
        }
    }
    
    /**
     * @brief Transform error value
     */
    template<typename F>
    Result<T, ErrorCode> mapError(F&& func) const noexcept {
        if (is_ok_) {
            return Result<T, ErrorCode>::ok(*reinterpret_cast<const T*>(&storage_));
        } else {
            auto new_error = func(error_.code);
            return Result<T, ErrorCode>::err(new_error, error_.message);
        }
    }
    
    /**
     * @brief Chain operations (flatmap)
     */
    template<typename F>
    auto andThen(F&& func) const noexcept -> decltype(func(std::declval<T>())) {
        using ReturnType = decltype(func(std::declval<T>()));
        if (is_ok_ && has_value_) {
            return func(*reinterpret_cast<const T*>(&storage_));
        } else {
            return ReturnType::err(error_.code, error_.message);
        }
    }
    
private:
    Result() noexcept : is_ok_(false), has_value_(false) {}
    
    void destroy() noexcept {
        if (is_ok_ && has_value_) {
            try {
                reinterpret_cast<T*>(&storage_)->~T();
            } catch (...) {
                // Suppress
            }
        }
    }
    
    bool is_ok_;
    bool has_value_;
    
    union {
        alignas(T) unsigned char storage_[sizeof(T)];
        ErrorInfo error_;
    };
};

/**
 * @brief Specialization for void return type
 */
template<>
class Result<void, ErrorCode> {
public:
    static Result ok() noexcept {
        Result r;
        r.is_ok_ = true;
        return r;
    }
    
    static Result err(ErrorCode code, const std::string& message = "") noexcept {
        Result r;
        r.is_ok_ = false;
        r.error_ = ErrorInfo(code, message);
        return r;
    }
    
    Result() noexcept : is_ok_(false) {}
    
    bool isSuccess() const noexcept { return is_ok_; }
    bool isError() const noexcept { return !is_ok_; }
    explicit operator bool() const noexcept { return is_ok_; }
    
    ErrorCode errorCode() const noexcept {
        return error_.code;
    }
    
    const ErrorInfo& errorInfo() const noexcept {
        return error_;
    }
    
    std::string errorMessage() const noexcept {
        return error_.formatFullError();
    }
    
private:
    bool is_ok_;
    ErrorInfo error_;
};

} // namespace cad
