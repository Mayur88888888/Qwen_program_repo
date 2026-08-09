/**
 * @file PersistenceManager.h
 * @brief Copy-on-Write file operations with atomic save and SHA-256 verification
 * @author Dr. Elias Voss
 * 
 * MANDATE: Never corrupt original file on save failure.
 * Write to .tmp, verify checksum, then atomic rename.
 */

#pragma once

#include "Core/Result.h"
#include "Core/MemoryGuard.h"
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <functional>

namespace cad {

/**
 * @brief SHA-256 checksum calculation
 */
class SHA256 {
public:
    /**
     * @brief Calculate hash of data buffer
     */
    static std::vector<uint8_t> compute(const uint8_t* data, size_t length) noexcept;
    
    /**
     * @brief Calculate hash of string
     */
    static std::vector<uint8_t> compute(const std::string& str) noexcept;
    
    /**
     * @brief Verify data against expected hash
     */
    static bool verify(const uint8_t* data, size_t length, 
                       const std::vector<uint8_t>& expected_hash) noexcept;
    
    /**
     * @brief Convert hash to hex string
     */
    static std::string toHexString(const std::vector<uint8_t>& hash) noexcept;
    
    /**
     * @brief Parse hex string to hash
     */
    static Result<std::vector<uint8_t>, ErrorCode> fromHexString(const std::string& hex) noexcept;
    
    static constexpr size_t HASH_SIZE = 32;  // 256 bits = 32 bytes
};

/**
 * @brief File format version for compatibility checking
 */
struct FileHeader {
    static constexpr uint32_t MAGIC_NUMBER = 0x4341444D;  // "CADM"
    static constexpr uint16_t CURRENT_VERSION = 2;
    
    uint32_t magic{MAGIC_NUMBER};
    uint16_t version{CURRENT_VERSION};
    uint16_t flags{0};
    uint64_t file_size{0};
    uint64_t timestamp{0};
    uint8_t sha256_hash[SHA256::HASH_SIZE];
    
    bool isValid() const noexcept {
        return magic == MAGIC_NUMBER && version <= CURRENT_VERSION;
    }
    
    std::vector<uint8_t> serialize() const noexcept;
    static Result<FileHeader, ErrorCode> deserialize(const std::vector<uint8_t>& data) noexcept;
};

/**
 * @brief Checkpoint for auto-save recovery
 */
struct Checkpoint {
    uint64_t timestamp;
    std::string filepath;
    std::vector<uint8_t> sha256_hash;
    uint64_t transaction_id;
    std::string description;
    
    bool isValid() const noexcept {
        return !filepath.empty() && !sha256_hash.empty();
    }
};

/**
 * @brief Persistence manager with atomic save protocol
 */
class PersistenceManager {
public:
    struct Config {
        std::string recovery_directory = "./recovery";
        std::string temp_directory = "./tmp";
        size_t auto_save_interval_seconds = 60;
        size_t max_checkpoints_to_keep = 10;
        bool enable_checksum_verification = true;
    };
    
    explicit PersistenceManager(Config config = Config()) noexcept;
    ~PersistenceManager() noexcept;
    
    /**
     * @brief Initialize persistence system (create directories, etc.)
     */
    Result<void, ErrorCode> initialize() noexcept;
    
    // ========== ATOMIC SAVE PROTOCOL ==========
    
    /**
     * @brief Save document atomically using Copy-on-Write
     * 
     * Protocol:
     * 1. Write to temporary file (.tmp)
     * 2. Calculate SHA-256 checksum
     * 3. Write checksum to footer
     * 4. Flush and sync filesystem
     * 5. Verify by re-reading and comparing checksum
     * 6. Atomic rename to final filename
     * 
     * If ANY step fails, original file remains untouched.
     */
    Result<void, ErrorCode> atomicSave(const std::string& filepath,
                                        const std::vector<uint8_t>& data,
                                        uint64_t transaction_id) noexcept;
    
    /**
     * @brief Load document with checksum verification
     */
    Result<std::vector<uint8_t>, ErrorCode> load(const std::string& filepath) noexcept;
    
    /**
     * @brief Check if file is valid (has correct checksum)
     */
    Result<bool, ErrorCode> verifyFile(const std::string& filepath) noexcept;
    
    // ========== AUTO-SAVE & CHECKPOINTING ==========
    
    /**
     * @brief Create checkpoint for crash recovery
     */
    Result<Checkpoint, ErrorCode> createCheckpoint(
        const std::vector<uint8_t>& data,
        uint64_t transaction_id,
        const std::string& description = "") noexcept;
    
    /**
     * @brief Get list of available checkpoints
     */
    std::vector<Checkpoint> getAvailableCheckpoints() const noexcept;
    
    /**
     * @brief Load from specific checkpoint
     */
    Result<std::vector<uint8_t>, ErrorCode> loadFromCheckpoint(
        const Checkpoint& checkpoint) noexcept;
    
    /**
     * @brief Load from most recent valid checkpoint
     */
    Result<std::vector<uint8_t>, ErrorCode> loadLatestCheckpoint() noexcept;
    
    /**
     * @brief Delete old checkpoints beyond retention limit
     */
    void cleanupOldCheckpoints() noexcept;
    
    // ========== RECOVERY OPERATIONS ==========
    
    /**
     * @brief Attempt to recover corrupted file
     * 
     * Recovery strategy:
     * 1. Try loading last valid checkpoint
     * 2. If no checkpoint, try parsing partial file structure
     * 3. Extract feature tree even if B-Rep is corrupted
     * 4. Return best-effort recovered data
     */
    Result<std::vector<uint8_t>, ErrorCode> attemptRecovery(
        const std::string& corrupted_filepath) noexcept;
    
    /**
     * @brief Repair file by removing corrupted sections
     */
    Result<std::vector<uint8_t>, ErrorCode> repairFile(
        const std::vector<uint8_t>& corrupted_data) noexcept;
    
    // ========== IMPORT/EXPORT ==========
    
    /**
     * @brief Export to STEP format
     */
    Result<void, ErrorCode> exportToSTEP(const std::string& filepath,
                                          const std::vector<uint8_t>& cad_data) noexcept;
    
    /**
     * @brief Import from STEP format
     */
    Result<std::vector<uint8_t>, ErrorCode> importFromSTEP(
        const std::string& filepath) noexcept;
    
    /**
     * @brief Export to STL format (for 3D printing)
     */
    Result<void, ErrorCode> exportToSTL(const std::string& filepath,
                                         const std::vector<uint8_t>& cad_data,
                                         bool binary = true) noexcept;
    
    /**
     * @brief Import from STL format
     */
    Result<std::vector<uint8_t>, ErrorCode> importFromSTL(
        const std::string& filepath) noexcept;
    
    /**
     * @brief Export to PDF (3D PDF)
     */
    Result<void, ErrorCode> exportToPDF(const std::string& filepath,
                                         const std::vector<uint8_t>& cad_data) noexcept;
    
    /**
     * @brief Export 2D to SVG
     */
    Result<void, ErrorCode> exportToSVG(const std::string& filepath,
                                         const std::vector<uint8_t>& cad_data_2d) noexcept;
    
    // ========== UTILITIES ==========
    
    /**
     * @brief Get file info
     */
    struct FileInfo {
        std::string filepath;
        uint64_t size_bytes;
        uint16_t version;
        uint64_t timestamp;
        bool is_valid;
        std::string error_message;
    };
    
    Result<FileInfo, ErrorCode> getFileInfo(const std::string& filepath) noexcept;
    
    /**
     * @brief Get temporary file path for given filename
     */
    std::string getTempPath(const std::string& original_path) const noexcept;
    
private:
    Result<void, ErrorCode> writeTemporaryFile(const std::string& temp_path,
                                                const std::vector<uint8_t>& data) noexcept;
    Result<void, ErrorCode> writeChecksum(const std::string& temp_path,
                                           const std::vector<uint8_t>& hash) noexcept;
    Result<bool, ErrorCode> verifyTemporaryFile(const std::string& temp_path,
                                                 const std::vector<uint8_t>& expected_hash) noexcept;
    Result<void, ErrorCode> atomicRename(const std::string& from, 
                                          const std::string& to) noexcept;
    void ensureRecoveryDirectory() noexcept;
    std::string generateCheckpointFilename() const noexcept;
    
    Config config_;
    std::vector<Checkpoint> known_checkpoints_;
};

} // namespace cad
