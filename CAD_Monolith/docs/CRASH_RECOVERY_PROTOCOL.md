# CAD MONOLITH - CRASH RECOVERY PROTOCOL
## Dr. Elias Voss - Auto-Save & Recovery System Design

---

## 1. AUTO-SAVE SYSTEM ARCHITECTURE

### 1.1 Checkpoint Creation Flow (Every 60 Seconds)

```cpp
/**
 * Pseudo-code for automatic checkpoint creation
 * Runs in background thread to avoid UI blocking
 */
void AutoSaveSystem::runCheckpointLoop() noexcept {
    auto last_save = std::chrono::steady_clock::now();
    
    while (session_.isRunning()) {
        // Wait for interval
        std::this_thread::sleep_for(std::chrono::seconds(60));
        
        // Skip if nothing changed
        if (!transactionMgr_.hasUnsavedChanges()) {
            continue;
        }
        
        // Create checkpoint
        auto checkpoint_result = createCheckpoint();
        
        if (checkpoint_result.isError()) {
            logError("Auto-save failed: " + checkpoint_result.errorMessage());
            // Do NOT notify user unless in Low Memory Mode
            continue;
        }
        
        // Register checkpoint
        recoveryMgr_.registerCheckpoint(checkpoint_result.value());
        
        // Cleanup old checkpoints (keep last 10)
        recoveryMgr_.cleanupOldCheckpoints();
        
        last_save = std::chrono::steady_clock::now();
    }
}

/**
 * Create checkpoint with full state serialization
 */
Result<Checkpoint, ErrorCode> AutoSaveSystem::createCheckpoint() noexcept {
    // Step 1: Serialize current state
    auto state = serializeModelState();
    if (state.isError()) return state.mapError([](auto){ return ErrorCode::MEMORY_EXHAUSTED; });
    
    // Step 2: Calculate SHA-256 checksum
    auto hash = SHA256::compute(state.value());
    
    // Step 3: Generate unique checkpoint filename
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    std::string checkpoint_path = config_.recovery_dir + "/checkpoint_" + 
                                   std::to_string(timestamp) + ".cadchk";
    
    // Step 4: Write using atomic save protocol
    Result<void, ErrorCode> write_result = persistenceMgr_.atomicSave(
        checkpoint_path, 
        state.value(),
        transactionMgr_.getCurrentTransactionId()
    );
    
    if (write_result.isError()) {
        // Clean up partial file
        std::remove(checkpoint_path.c_str());
        return Result<Checkpoint, ErrorCode>::err(write_result.errorCode(), 
                                                   "Failed to write checkpoint");
    }
    
    // Step 5: Return checkpoint info
    Checkpoint cp;
    cp.timestamp = timestamp;
    cp.filepath = checkpoint_path;
    cp.sha256_hash = hash;
    cp.transaction_id = transactionMgr_.getCurrentTransactionId();
    cp.description = "Auto-save at " + formatTimestamp(timestamp);
    
    return Result<Checkpoint, ErrorCode>::ok(cp);
}
```

---

## 2. CORRUPTED FILE RECOVERY FLOW

### 2.1 Load Failure Detection

```cpp
/**
 * When opening a .cad file fails, trigger recovery protocol
 */
Result<Document, ErrorCode> DocumentLoader::loadWithRecovery(
    const std::string& filepath) noexcept 
{
    // Attempt normal load first
    auto result = loadNormal(filepath);
    
    if (result.isSuccess()) {
        return result;  // Success - no recovery needed
    }
    
    ErrorCode error = result.errorCode();
    
    // Determine if recovery is appropriate
    bool needs_recovery = 
        error == ErrorCode::FILE_CORRUPTED ||
        error == ErrorCode::CHECKSUM_MISMATCH ||
        error == ErrorCode::VERSION_MISMATCH;
    
    if (!needs_recovery) {
        return result;  // Different error (e.g., file not found)
    }
    
    // Log corruption details
    logError("File corruption detected: " + filepath);
    logError("Error code: " + errorCodeToString(error));
    
    // Trigger recovery protocol
    return attemptRecoveryProtocol(filepath, error);
}
```

### 2.2 Recovery Protocol Steps

```cpp
/**
 * Multi-stage recovery attempt
 * Escalates through increasingly aggressive recovery strategies
 */
Result<Document, ErrorCode> DocumentLoader::attemptRecoveryProtocol(
    const std::string& corrupted_path, 
    ErrorCode original_error) noexcept 
{
    RecoveryReport report;
    report.original_file = corrupted_path;
    report.original_error = original_error;
    
    // ========== STAGE 1: Load Last Checkpoint ==========
    logInfo("[RECOVERY] Stage 1: Attempting checkpoint recovery...");
    
    auto checkpoints = persistenceMgr_.getAvailableCheckpoints();
    
    for (const auto& checkpoint : checkpoints) {
        auto cp_data = persistenceMgr_.loadFromCheckpoint(checkpoint);
        
        if (cp_data.isSuccess()) {
            // Verify checkpoint integrity
            if (verifyCheckpointData(cp_data.value(), checkpoint.sha256_hash)) {
                report.recovery_method = "CHECKPOINT";
                report.checkpoint_used = checkpoint.filepath;
                
                auto doc = deserializeDocument(cp_data.value());
                if (doc.isSuccess()) {
                    doc.value().setRecoveryReport(report);
                    logInfo("[RECOVERY] Successfully recovered from checkpoint");
                    return doc;
                }
            }
        }
    }
    
    logWarning("[RECOVERY] Stage 1 failed: No valid checkpoints found");
    
    // ========== STAGE 2: Attempt File Repair ==========
    logInfo("[RECOVERY] Stage 2: Attempting file repair...");
    
    auto raw_data = readFileRaw(corrupted_path);
    if (raw_data.isSuccess()) {
        auto repaired = persistenceMgr_.repairFile(raw_data.value());
        
        if (repaired.isSuccess()) {
            auto doc = deserializeDocument(repaired.value());
            if (doc.isSuccess()) {
                report.recovery_method = "FILE_REPAIR";
                doc.value().setRecoveryReport(report);
                logInfo("[RECOVERY] Successfully repaired file");
                return doc;
            }
        }
    }
    
    logWarning("[RECOVERY] Stage 2 failed: File repair unsuccessful");
    
    // ========== STAGE 3: Feature Tree Extraction ==========
    logInfo("[RECOVERY] Stage 3: Extracting feature tree only...");
    
    // Parse only the feature tree section (more resilient than B-Rep)
    auto feature_tree = extractFeatureTree(corrupted_path);
    
    if (feature_tree.isSuccess()) {
        // Create skeleton document with feature tree
        Document skeleton;
        skeleton.setFeatureTree(feature_tree.value());
        skeleton.setRecoveryReport(report);
        skeleton.setStatus(DocumentStatus::PARTIAL_RECOVERY);
        
        logInfo("[RECOVERY] Stage 3 succeeded: Feature tree recovered");
        logWarning("[RECOVERY] B-Rep geometry lost - rebuild required");
        
        return Result<Document, ErrorCode>::ok(std::move(skeleton));
    }
    
    logError("[RECOVERY] Stage 3 failed: Could not extract feature tree");
    
    // ========== STAGE 4: Complete Recovery Failure ==========
    report.recovery_method = "NONE";
    report.success = false;
    
    logCritical("[RECOVERY] ALL STAGES FAILED - Manual intervention required");
    
    return Result<Document, ErrorCode>::err(
        ErrorCode::FILE_CORRUPTED,
        "All recovery attempts failed. File may be permanently lost."
    );
}
```

---

## 3. FILE REPAIR ALGORITHM

```cpp
/**
 * Attempt to repair corrupted file data
 * Strategy: Identify and remove corrupted sections, preserve what's valid
 */
Result<std::vector<uint8_t>, ErrorCode> PersistenceManager::repairFile(
    const std::vector<uint8_t>& corrupted_data) noexcept 
{
    if (corrupted_data.size() < sizeof(FileHeader)) {
        return Result<std::vector<uint8_t>, ErrorCode>::err(
            ErrorCode::FILE_CORRUPTED, "File too small for header");
    }
    
    // Step 1: Parse and validate header
    FileHeader header;
    auto header_result = FileHeader::deserialize(
        std::vector<uint8_t>(corrupted_data.begin(), 
                            corrupted_data.begin() + sizeof(FileHeader)));
    
    if (header_result.isError()) {
        // Try to reconstruct header from backup location
        header = reconstructHeader(corrupted_data);
    } else {
        header = header_result.value();
    }
    
    // Step 2: Identify corrupted sections using checksums
    std::vector<DataSection> sections = identifySections(corrupted_data);
    std::vector<DataSection> valid_sections;
    
    for (auto& section : sections) {
        if (verifySectionChecksum(section)) {
            valid_sections.push_back(section);
        } else {
            logWarning("Corrupted section identified: " + section.name);
        }
    }
    
    // Step 3: Rebuild file from valid sections
    std::vector<uint8_t> repaired_data;
    repaired_data.reserve(corrupted_data.size());
    
    // Write header
    auto header_bytes = header.serialize();
    repaired_data.insert(repaired_data.end(), header_bytes.begin(), header_bytes.end());
    
    // Write valid sections
    for (const auto& section : valid_sections) {
        repaired_data.insert(repaired_data.end(), 
                            section.data.begin(), 
                            section.data.end());
    }
    
    // Pad to expected size if possible
    if (repaired_data.size() < header.file_size) {
        logWarning("Repaired file smaller than original");
        // Add padding marker
        uint64_t padding_size = header.file_size - repaired_data.size();
        addPaddingMarker(repaired_data, padding_size);
    }
    
    // Step 4: Recalculate checksum for repaired file
    auto new_hash = SHA256::compute(repaired_data);
    updateHeaderHash(repaired_data, new_hash);
    
    return Result<std::vector<uint8_t>, ErrorCode>::ok(std::move(repaired_data));
}
```

---

## 4. CHECKPOINT CLEANUP STRATEGY

```cpp
/**
 * Maintain checkpoint retention policy
 * Keep last N checkpoints, delete older ones
 */
void PersistenceManager::cleanupOldCheckpoints() noexcept {
    auto checkpoints = getAvailableCheckpoints();
    
    if (checkpoints.size() <= config_.max_checkpoints_to_keep) {
        return;  // Within limit
    }
    
    // Sort by timestamp (newest first)
    std::sort(checkpoints.begin(), checkpoints.end(),
              [](const Checkpoint& a, const Checkpoint& b) {
                  return a.timestamp > b.timestamp;
              });
    
    // Delete old checkpoints
    for (size_t i = config_.max_checkpoints_to_keep; i < checkpoints.size(); ++i) {
        std::remove(checkpoints[i].filepath.c_str());
        logInfo("Cleaned up old checkpoint: " + checkpoints[i].filepath);
    }
}
```

---

## 5. RECOVERY REPORT STRUCTURE

```cpp
/**
 * Report generated after recovery attempt
 * Informs user of what was recovered and what was lost
 */
struct RecoveryReport {
    std::string original_file;
    ErrorCode original_error;
    std::string recovery_method;  // "CHECKPOINT", "FILE_REPAIR", "FEATURE_TREE", "NONE"
    std::string checkpoint_used;
    bool success{false};
    
    // Detailed loss report
    struct LossReport {
        bool geometry_lost{false};
        bool constraints_lost{false};
        bool materials_lost{false};
        bool annotations_lost{false};
        std::vector<std::string> missing_features;
    };
    
    LossReport losses;
    
    std::string generateUserReport() const noexcept {
        std::ostringstream oss;
        oss << "=== RECOVERY REPORT ===\n";
        oss << "Original File: " << original_file << "\n";
        oss << "Error: " << errorCodeToString(original_error) << "\n";
        oss << "Recovery Method: " << recovery_method << "\n";
        oss << "Success: " << (success ? "YES" : "NO") << "\n";
        
        if (!success) {
            oss << "\n*** CRITICAL: File could not be recovered ***\n";
            oss << "Recommendation: Restore from external backup\n";
        } else if (!losses.geometry_lost && !losses.constraints_lost) {
            oss << "\nFull recovery achieved - no data loss detected.\n";
        } else {
            oss << "\nPartial recovery - some data was lost:\n";
            if (losses.geometry_lost) oss << "  - 3D Geometry\n";
            if (losses.constraints_lost) oss << "  - Parametric Constraints\n";
            if (losses.materials_lost) oss << "  - Material Definitions\n";
            if (losses.annotations_lost) oss << "  - Annotations/Dimensions\n";
            
            if (!losses.missing_features.empty()) {
                oss << "Missing Features:\n";
                for (const auto& feat : losses.missing_features) {
                    oss << "  - " << feat << "\n";
                }
            }
        }
        
        return oss.str();
    }
};
```

---

## 6. USER NOTIFICATION FLOW

```cpp
/**
 * Notify user of recovery status after document load
 */
void ApplicationUI::showRecoveryNotification(const RecoveryReport& report) noexcept {
    if (report.success) {
        if (report.losses.geometry_lost || report.losses.constraints_lost) {
            // Partial recovery - show warning
            showWarningDialog(
                "Partial Recovery Completed",
                report.generateUserReport(),
                { "Save As...", "Continue", "Close Document" }
            );
        } else {
            // Full recovery - subtle notification
            showInfoToast("Document recovered successfully from auto-save");
        }
    } else {
        // Complete failure - critical alert
        showErrorDialog(
            "Recovery Failed",
            report.generateUserReport() + "\n\n" +
            "The file could not be recovered. Would you like to:\n" +
            "1. Try opening a backup copy\n" +
            "2. Start a new document\n" +
            "3. Exit application",
            { "Open Backup", "New Document", "Exit" }
        );
    }
}
```

---

## 7. KEY RECOVERY METRICS

| Metric | Target | Measurement |
|--------|--------|-------------|
| Auto-Save Interval | 60 seconds | Time between checkpoints |
| Checkpoint Write Time | < 2 seconds | For typical 100MB model |
| Recovery Success Rate | > 95% | Of corruption incidents |
| Max Data Loss | 60 seconds | Worst case: one auto-save interval |
| Checkpoint Retention | 10 checkpoints | ~10 minutes of history |
| Recovery Time | < 30 seconds | From crash to working state |

---

## 8. TESTING REQUIREMENTS

1. **Simulated Corruption Tests**
   - Corrupt random bytes in saved file
   - Truncate file at various offsets
   - Overwrite header with garbage
   - Verify recovery succeeds or fails gracefully

2. **Crash Simulation Tests**
   - Kill process during save operation
   - Kill process during kernel computation
   - Restart and verify recovery from checkpoint

3. **Disk Full Scenarios**
   - Fill disk to capacity
   - Verify auto-save handles gracefully (no crash)
   - Verify low-memory mode activates

4. **Power Failure Simulation**
   - Interrupt filesystem operations mid-write
   - Verify atomic rename protects original file
