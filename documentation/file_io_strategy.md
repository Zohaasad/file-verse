# File I/O Strategy

## 1. Filesystem Layout

Each `.omni` file is a structured binary container divided into logical regions:

| Section | Purpose | Size/Offset |
|----------|----------|-------------|
| Header (`OMNIHeader`) | Basic config and metadata | Fixed (512 bytes) |
| Bitmap | Tracks used/free blocks | `bitmap_offset` |
| Metadata region | Stores `MetaEntry` records | `metadata_offset` |
| Data blocks | File and directory contents | `blocks_offset` |

Offsets are computed during initialization (`fs_init`).

## 2. File Operations

### Creation
`file_create()` allocates metadata and data blocks, updates the path index, and writes content sequentially.

### Reading
`file_read()`:
1. Locates file metadata via `path_index`.
2. Reads block indices and reconstructs content.
3. Allocates a buffer returned to the caller.

### Editing
`file_edit()`:
- Seeks to the specified byte index.
- Overwrites the region with the new data.
- Updates `modified_time`.

### Truncation
`file_truncate()` supports both shrinking and expanding files:
- Shrinking frees trailing blocks.
- Expanding allocates new zero-filled blocks.

### Renaming / Deletion
`file_rename()` updates `MetaEntry.name` and the path index.
`file_delete()` clears metadata and releases occupied blocks in the bitmap.

## 3. Directory Operations

Directories are treated as special `MetaEntry` objects (`type = directory`).
They can contain other entries through parent indices.
- `dir_create()` adds a new metadata node.
- `dir_list()` enumerates children under a given parent.
- `dir_delete()` validates emptiness before removing.

## 4. Buffer Management

All buffers returned to the user are dynamically allocated and must be released via `free_buffer()`.  
This ensures consistent memory lifecycle control between API and client code.

## 5. Error Handling

Each public function returns an integer code:
- `0` → Success.
- Non-zero → Error (retrieved via `get_error_message()`).

This strategy avoids exceptions and keeps the API C-compatible.
