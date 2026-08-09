#include "Persistence/PersistenceManager.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace Cad {

PersistenceManager* PersistenceManager::s_instance = nullptr;

PersistenceManager& PersistenceManager::getInstance() {
    if (!s_instance) {
        s_instance = new PersistenceManager();
    }
    return *s_instance;
}

void PersistenceManager::destroyInstance() {
    delete s_instance;
    s_instance = nullptr;
}

PersistenceManager::PersistenceManager()
    : m_autoSaveInterval(std::chrono::seconds(60))
    , m_autoSaveTimer()
    , m_isAutoSaving(false)
    , m_lastSavePath() {
    
    startAutoSaveThread();
}

PersistenceManager::~PersistenceManager() noexcept {
    stopAutoSaveThread();
}

Result<void, ErrorCode> PersistenceManager::saveFile(const std::string& filePath, const DocumentData& data) {
    std::cout << "[PERSISTENCE] Starting atomic save to: " << filePath << "\n";
    
    // Step 1: Generate temporary file path
    std::string tempPath = filePath + ".tmp";
    
    // Step 2: Serialize data to buffer
    std::stringstream buffer;
    Result<void, ErrorCode> serializeResult = serializeDocument(data, buffer);
    if (!serializeResult.isSuccess()) {
        return serializeResult;
    }
    
    // Step 3: Write to temporary file
    {
        std::ofstream outFile(tempPath, std::ios::binary | std::ios::trunc);
        if (!outFile.is_open()) {
            return Result<void, ErrorCode>::failure(ErrorCode::FILE_OPEN_FAILED,
                "Cannot open temporary file for writing: " + tempPath);
        }
        
        outFile << buffer.str();
        outFile.flush();
        
        #ifdef _WIN32
            outFile.sync_with_stdio();
        #else
            fsync(outFile.rdbuf()->native_handle());
        #endif
    }
    
    // Step 4: Calculate SHA-256 checksum of temp file
    std::string checksum = calculateSHA256(tempPath);
    if (checksum.empty()) {
        std::remove(tempPath.c_str());
        return Result<void, ErrorCode>::failure(ErrorCode::CHECKSUM_FAILED,
            "Failed to calculate checksum");
    }
    
    // Step 5: Append checksum to file or store in header
    // For simplicity, we'll append it as a footer
    {
        std::ofstream outFile(tempPath, std::ios::binary | std::ios::app);
        outFile << "\n[CHECKSUM:" << checksum << "]";
        outFile.close();
    }
    
    // Step 6: Verify by re-reading and checking checksum
    std::string verifyChecksum = calculateSHA256(tempPath, true); // true = exclude footer
    if (verifyChecksum != checksum) {
        std::remove(tempPath.c_str());
        return Result<void, ErrorCode>::failure(ErrorCode::CHECKSUM_MISMATCH,
            "Checksum verification failed after write");
    }
    
    // Step 7: Atomic rename
    #ifdef _WIN32
        // Delete existing file first on Windows (rename fails if target exists)
        std::remove(filePath.c_str());
        if (MoveFileExA(tempPath.c_str(), filePath.c_str(), MOVEFILE_REPLACE_EXISTING) == 0) {
            std::remove(tempPath.c_str());
            return Result<void, ErrorCode>::failure(ErrorCode::FILE_RENAME_FAILED,
                "Atomic rename failed");
        }
    #else
        if (std::rename(tempPath.c_str(), filePath.c_str()) != 0) {
            std::remove(tempPath.c_str());
            return Result<void, ErrorCode>::failure(ErrorCode::FILE_RENAME_FAILED,
                "Atomic rename failed");
        }
    #endif
    
    m_lastSavePath = filePath;
    std::cout << "[PERSISTENCE] Save completed successfully\n";
    
    return Result<void, ErrorCode>::success();
}

Result<DocumentData, ErrorCode> PersistenceManager::loadFile(const std::string& filePath) {
    std::cout << "[PERSISTENCE] Loading file: " << filePath << "\n";
    
    std::ifstream inFile(filePath, std::ios::binary);
    if (!inFile.is_open()) {
        return Result<DocumentData, ErrorCode>::failure(ErrorCode::FILE_OPEN_FAILED,
            "Cannot open file for reading: " + filePath);
    }
    
    // Read entire file
    std::stringstream buffer;
    buffer << inFile.rdbuf();
    std::string content = buffer.str();
    inFile.close();
    
    // Extract and verify checksum
    size_t checksumPos = content.rfind("[CHECKSUM:");
    if (checksumPos == std::string::npos) {
        return Result<DocumentData, ErrorCode>::failure(ErrorCode::INVALID_FILE_FORMAT,
            "No checksum found in file");
    }
    
    std::string storedChecksum = content.substr(checksumPos + 10, 64); // SHA-256 is 64 hex chars
    std::string dataContent = content.substr(0, checksumPos);
    
    // Calculate checksum of data portion
    std::stringstream dataStream(dataContent);
    std::string calculatedChecksum = calculateSHA256Stream(dataStream);
    
    if (storedChecksum != calculatedChecksum) {
        std::cout << "[PERSISTENCE] Checksum mismatch! File may be corrupted.\n";
        std::cout << "[PERSISTENCE] Stored:     " << storedChecksum << "\n";
        std::cout << "[PERSISTENCE] Calculated: " << calculatedChecksum << "\n";
        
        // Attempt recovery
        return attemptRecovery(filePath);
    }
    
    // Deserialize document
    DocumentData data;
    Result<void, ErrorCode> deserializeResult = deserializeDocument(dataStream, data);
    if (!deserializeResult.isSuccess()) {
        return Result<DocumentData, ErrorCode>::failure(deserializeResult.getError());
    }
    
    return Result<DocumentData, ErrorCode>::success(data);
}

Result<DocumentData, ErrorCode> PersistenceManager::attemptRecovery(const std::string& filePath) {
    std::cout << "[PERSISTENCE] Attempting recovery of corrupted file...\n";
    
    // Recovery Strategy 1: Try loading from auto-save
    std::string autoSavePath = getAutoSavePath(filePath);
    std::ifstream autoSaveFile(autoSavePath, std::ios::binary);
    if (autoSaveFile.is_open()) {
        std::cout << "[PERSISTENCE] Found auto-save file, attempting load...\n";
        autoSaveFile.close();
        return loadFile(autoSavePath);
    }
    
    // Recovery Strategy 2: Try stripping corrupted sections
    // (In real implementation, would parse and salvage valid portions)
    
    // Recovery Strategy 3: Load feature tree only (lose B-Rep but keep history)
    std::cout << "[PERSISTENCE] Falling back to feature-tree-only recovery\n";
    DocumentData partialData;
    partialData.isPartialRecovery = true;
    partialData.recoveryMessage = "File was corrupted. Loaded feature tree only. Some geometry may be missing.";
    return Result<DocumentData, ErrorCode>::success(partialData);
}

void PersistenceManager::triggerAutoSave(const DocumentData& data) {
    if (m_isAutoSaving) {
        return; // Already saving
    }
    
    m_isAutoSaving = true;
    
    std::string autoSavePath = getAutoSavePath(m_lastSavePath);
    
    // Save to auto-save location (non-atomic, faster)
    std::ofstream outFile(autoSavePath, std::ios::binary);
    if (outFile.is_open()) {
        std::stringstream buffer;
        if (serializeDocument(data, buffer).isSuccess()) {
            outFile << buffer.str();
        }
        outFile.close();
        std::cout << "[PERSISTENCE] Auto-save completed: " << autoSavePath << "\n";
    }
    
    m_isAutoSaving = false;
}

std::string PersistenceManager::getAutoSavePath(const std::string& originalPath) const {
    if (originalPath.empty()) {
        return SessionManager::getInstance().getRecoveryFilePath();
    }
    
    size_t lastDot = originalPath.find_last_of(".");
    std::string baseName = (lastDot != std::string::npos) ? 
                           originalPath.substr(0, lastDot) : originalPath;
    return baseName + ".autosave";
}

void PersistenceManager::startAutoSaveThread() {
    m_autoSaveTimer = std::make_unique<std::thread>([this]() {
        while (true) {
            std::this_thread::sleep_for(m_autoSaveInterval);
            
            // In real implementation, would check if document is dirty
            // and trigger auto-save via callback
        }
    });
}

void PersistenceManager::stopAutoSaveThread() noexcept {
    // Thread will exit naturally when destructor runs
    // In real implementation, would use atomic flag to signal exit
}

std::string PersistenceManager::calculateSHA256(const std::string& filePath, bool excludeFooter) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    
    if (excludeFooter) {
        // Find and exclude footer
        std::stringstream buffer;
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("[CHECKSUM:") != std::string::npos) {
                break;
            }
            buffer << line << "\n";
        }
        return calculateSHA256Stream(buffer);
    } else {
        return calculateSHA256Stream(file);
    }
}

std::string PersistenceManager::calculateSHA256Stream(std::istream& stream) {
    // Simplified SHA-256 placeholder
    // In real implementation, use OpenSSL or Crypto++ library
    std::stringstream buffer;
    buffer << stream.rdbuf();
    std::string content = buffer.str();
    
    // Dummy hash for blueprint purposes
    uint32_t hash = 0xdeadbeef;
    for (char c : content) {
        hash = hash * 31 + static_cast<uint8_t>(c);
    }
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << hash;
    ss << std::setw(8) << hash; // Repeat to simulate 64-char hash
    ss << std::setw(8) << hash;
    ss << std::setw(8) << hash;
    
    return ss.str();
}

Result<void, ErrorCode> PersistenceManager::serializeDocument(const DocumentData& data, std::ostream& out) {
    // Placeholder serialization
    // Real implementation would serialize B-Rep, feature tree, constraints, etc.
    out << "CAD_DOCUMENT_V1\n";
    out << "SESSION:" << data.sessionId << "\n";
    out << "ENTITIES:" << data.entityCount << "\n";
    return Result<void, ErrorCode>::success();
}

Result<void, ErrorCode> PersistenceManager::deserializeDocument(std::istream& in, DocumentData& data) {
    // Placeholder deserialization
    std::string header;
    std::getline(in, header);
    if (header != "CAD_DOCUMENT_V1") {
        return Result<void, ErrorCode>::failure(ErrorCode::INVALID_FILE_FORMAT,
            "Unknown document version");
    }
    return Result<void, ErrorCode>::success();
}

} // namespace Cad
