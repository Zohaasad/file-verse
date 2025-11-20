# OFS – Omni File Server: File I/O Strategy

This document explains the file input/output and data handling strategy in the Omni File Server (OFS). It details how files and directories are stored, accessed, and managed internally, including concurrency, buffer management, and error handling.

## Table of Contents

1. [Overview](#overview)
2. [Filesystem Layout](#filesystem-layout)
3. [File Storage Model](#file-storage-model)
4. [Directory Management](#directory-management)
5. [File Operations](#file-operations)
6. [Buffer Management](#buffer-management)
7. [Data Consistency and Concurrency](#data-consistency-and-concurrency)
8. [Error Handling and Validation](#error-handling-and-validation)
9. [Summary](#summary)

---

## Overview

OFS provides a virtual filesystem loaded from a precompiled `.omni` binary file. Clients interact with files and directories using a session-based API.

### Key Principles

- Thread-safe operations for multiple concurrent clients.
- Separation of metadata and file contents.
- JSON-based responses for structured client-server communication.
- Atomic operations to prevent partial writes or data corruption.

---

## Filesystem Layout

Each `.omni` file is a structured binary container divided into logical regions:

| Section | Purpose | Offset / Size |
|---------|---------|---------------|
| Header (`OMNIHeader`) | Basic configuration and metadata | Fixed: 512 bytes |
| Bitmap | Tracks used/free data blocks | `bitmap_offset` |
| Metadata Region | Stores `MetaEntry` records | `metadata_offset` |
| Data Blocks | File and directory contents | `blocks_offset` |

- **Offsets** are computed during filesystem initialization (`fs_init()`).
- **Bitmap:** Each bit corresponds to a data block; 1 = used, 0 = free.
- **Metadata Region:** Fixed-size entries enable deterministic on-disk layout.
- **Data Blocks:** Store actual file content in a contiguous or segmented manner.

---

## File Storage Model

Each file is represented by a `FileEntry` structure:

```cpp
struct FileEntry {
    std::string name;       // File or directory name
    uint8_t type;           // File or directory
    size_t size;            // File size in bytes
    std::string owner;      // Owner username
    uint32_t permissions;   // Unix-style permission flags
    uint64_t created_time;  // Epoch timestamp
    uint64_t modified_time; // Epoch timestamp
};
```

- File contents are stored in data blocks referenced by metadata.
- Partial edits are supported via offsets and temporary buffers.
- Internal `FileMetadata` wraps `FileEntry` with additional runtime info:

```cpp
struct FileMetadata {
    FileEntry entry;
    std::string path;       // Full path in FS
    size_t blocks_used;     // Number of storage blocks occupied
};
```

---

## Directory Management

Directories are special `MetaEntry` objects (type = directory) and can contain nested files or directories.

### Operations include:

- `dir_create(path)`: Adds a new directory metadata node.
- `dir_delete(path)`: Deletes an empty directory.
- `dir_exists(path)`: Checks for existence.
- `dir_list(path)`: Returns all files and subdirectories.

### Notes:

- Each directory entry contains owner and permission metadata.
- JSON escaping ensures safe serialization for client responses.

---

## File Operations

### 1. Create File (`file_create(session, path, content, size)`)

- Allocates metadata and blocks.
- Writes content sequentially into the data region.
- Updates path indices for quick lookup.

### 2. Read File (`file_read(session, path, &buffer, &size)`)

- Locates metadata via path index.
- Reads data blocks and copies content into a dynamically allocated buffer.

### 3. Edit File (`file_edit(session, path, new_data, size, offset)`)

- Seeks to a specific byte offset.
- Overwrites data safely while locking the file.
- Updates `modified_time`.

### 4. Truncate File (`file_truncate(session, path, new_size)`)

- **Shrinking:** frees trailing blocks.
- **Expanding:** allocates zero-filled blocks.

### 5. Rename File (`file_rename(session, old_path, new_path)`)

- Updates `MetaEntry.name` and path index atomically.

### 6. Delete File (`file_delete(session, path)`)

- Frees memory, metadata, and occupied bitmap blocks.
- Validates permissions before deletion.

---

## Buffer Management

- All returned file buffers are dynamically allocated.
- Clients must call `free_buffer()` to release memory after use.
- Temporary buffers are used during edits to ensure atomic writes.
- Guarantees consistent memory lifecycle between API and client code.

---

## Data Consistency and Concurrency

- **TSQueue:** Thread-safe queue for client requests.
- **Session Mutex (`session_mtx`):** Protects session and metadata access.
- **Atomic Operations:** Each API call locks necessary resources to prevent race conditions.
- **Multi-client support:** Multiple simultaneous reads/writes do not corrupt the filesystem.

---

### Memory vs Disk Access

- Metadata (`FileMetadata`, `MetaEntry`) and path indices are loaded into memory at server start for fast lookup.
- Actual file content is read from disk into temporary buffers only during read/edit operations.
- Bitmap and header remain in memory for quick free space tracking.

## Error Handling and Validation

Each public API function returns an integer code:

- `0` → Success
- Non-zero → Error (retrieved via `get_error_message()`)

### Common errors:

- `ERROR_INVALID_SESSION`: Session is not active or unauthorized.
- `ERROR_FILE_NOT_FOUND`: File does not exist.
- `ERROR_DIR_NOT_FOUND`: Directory does not exist.
- `ERROR_ACCESS_DENIED`: Permission violation.

### JSON Response Example:

```json
{
  "status": "error",
  "operation": "read_file",
  "request_id": "0",
  "error_message": "File not found",
  "data": ""
}
```

- Ensures clients always receive structured and informative feedback.

---

## Summary

The OFS File I/O strategy ensures:

- **Structured storage** in a deterministic `.omni` layout.
- **Atomic and thread-safe** read/write operations.
- **Concurrent client support** without data corruption.
- **Efficient buffer and memory management** for performance.
- **Clear error reporting** through return codes and JSON responses.

This strategy enables OFS to be a robust, modular, and multi-user virtual filesystem.
