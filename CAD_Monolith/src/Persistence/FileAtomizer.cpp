#include "Persistence/FileAtomizer.h"

namespace Cad {

FileAtomizer::FileAtomizer(const std::string& targetPath)
    : m_targetPath(targetPath)
    , m_tempPath(targetPath + ".atomizer_tmp")
    , m_isCommitted(false)
    , m_fileHandle(nullptr) {
}

FileAtomizer::~FileAtomizer() noexcept {
    if (!m_isCommitted) {
        rollback();
    }
}

Result<void, ErrorCode> FileAtomizer::beginWrite() {
    #ifdef _WIN32
        m_fileHandle = CreateFileA(
            m_tempPath.c_str(),
            GENERIC_WRITE,
            0, // No sharing - exclusive access
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_WRITE_THROUGH,
            nullptr
        );
        
        if (m_fileHandle == INVALID_HANDLE_VALUE) {
            return Result<void, ErrorCode>::failure(ErrorCode::FILE_OPEN_FAILED,
                "Failed to create temporary file");
        }
    #else
        m_fd = open(m_tempPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (m_fd == -1) {
            return Result<void, ErrorCode>::failure(ErrorCode::FILE_OPEN_FAILED,
                "Failed to create temporary file");
        }
    #endif
    
    return Result<void, ErrorCode>::success();
}

Result<void, ErrorCode> FileAtomizer::write(const void* data, size_t size) {
    #ifdef _WIN32
        DWORD bytesWritten;
        if (!WriteFile(m_fileHandle, data, static_cast<DWORD>(size), &bytesWritten, nullptr)) {
            return Result<void, ErrorCode>::failure(ErrorCode::FILE_WRITE_FAILED,
                "Failed to write data to temporary file");
        }
    #else
        ssize_t written = ::write(m_fd, data, size);
        if (written != static_cast<ssize_t>(size)) {
            return Result<void, ErrorCode>::failure(ErrorCode::FILE_WRITE_FAILED,
                "Failed to write data to temporary file");
        }
    #endif
    
    return Result<void, ErrorCode>::success();
}

Result<void, ErrorCode> FileAtomizer::commit() {
    if (m_isCommitted) {
        return Result<void, ErrorCode>::failure(ErrorCode::ALREADY_COMMITTED,
            "File has already been committed");
    }
    
    // Sync to disk
    #ifdef _WIN32
        FlushFileBuffers(m_fileHandle);
        CloseHandle(m_fileHandle);
        m_fileHandle = nullptr;
        
        // Delete existing target file
        std::remove(m_targetPath.c_str());
        
        // Atomic rename
        if (MoveFileExA(m_tempPath.c_str(), m_targetPath.c_str(), MOVEFILE_REPLACE_EXISTING) == 0) {
            return Result<void, ErrorCode>::failure(ErrorCode::FILE_RENAME_FAILED,
                "Failed to atomically rename temporary file");
        }
    #else
        fsync(m_fd);
        close(m_fd);
        m_fd = -1;
        
        // Atomic rename
        if (std::rename(m_tempPath.c_str(), m_targetPath.c_str()) != 0) {
            return Result<void, ErrorCode>::failure(ErrorCode::FILE_RENAME_FAILED,
                "Failed to atomically rename temporary file");
        }
    #endif
    
    m_isCommitted = true;
    return Result<void, ErrorCode>::success();
}

void FileAtomizer::rollback() noexcept {
    #ifdef _WIN32
        if (m_fileHandle && m_fileHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_fileHandle);
            m_fileHandle = nullptr;
        }
        std::remove(m_tempPath.c_str());
    #else
        if (m_fd != -1) {
            close(m_fd);
            m_fd = -1;
        }
        std::remove(m_tempPath.c_str());
    #endif
}

bool FileAtomizer::isCommitted() const {
    return m_isCommitted;
}

const std::string& FileAtomizer::getTempPath() const {
    return m_tempPath;
}

const std::string& FileAtomizer::getTargetPath() const {
    return m_targetPath;
}

} // namespace Cad
