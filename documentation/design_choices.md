# Design Choices

## 1. Overall Architecture

The Omni File System (OFS) is designed as a lightweight, modular virtual file system implemented in C++.  
It simulates a persistent, block-based filesystem stored inside a single `.omni` binary file.  
Core responsibilities are divided into layers:

- **Data Structures Layer** (`data_structures/`)  
  Provides generic containers (e.g., `SimpleHashMap`) for in-memory indexing.
- **Core Layer** (`core/`)  
  Implements the core filesystem logic (`ofs_instance.hpp`, `meta_entry.hpp`, `ofs_core.cpp`).
- **Interface Layer**  
  Exposes user-facing API functions (e.g., `fs_format`, `file_create`, `user_login`).
- **Testing Layer** (`main.cpp`)  
  Runs a comprehensive end-to-end test sequence simulating full usage of the filesystem.

## 2. Data Structures

### SimpleHashMap
A templated hash map implemented from scratch to avoid STL overhead.  
Used for:
- Fast lookup of **user indices**, **path mappings**, and **session references**.
- Reducing external dependencies.

Key operations (`insert`, `erase`, `contains`, `find`) are designed for O(1) average-time complexity.

### MetaEntry
Represents a file or directory record.  
Each entry occupies **exactly 72 bytes**, ensuring predictable on-disk alignment and fixed-size metadata blocks.  
Fields like `permissions`, `timestamps`, and `owner_id` are stored for Unix-like semantics.

### FSInstance
Holds all runtime state:
- File handle for the `.omni` file.
- Metadata vectors.
- Bitmaps for free space tracking.
- Session/user maps.
- Encoding and security buffers.

By encapsulating all context in `FSInstance`, multi-session handling becomes safe and extendable.

## 3. Memory and Performance

- **Hash-based caching:** Reduces repeated lookups of files and sessions.
- **Minimal heap usage:** Vectors and static buffers avoid unnecessary allocations.
- **Fixed-size metadata blocks:** Guarantees consistent on-disk layout and fast seek operations.

## 4. Modularity

Each logical unit (user management, metadata, I/O, sessions) is decoupled via extern function APIs.  
This enables:
- Easier testing.
- Replacement of components (e.g., `SimpleHashMap` → `std::unordered_map`).
- Future extension (e.g., networked FS backend).

## 5. Security and Robustness

- Private keys and encoding maps are pre-zeroed to avoid uninitialized memory exposure.
- User credentials and roles (admin/normal) enforce access control.
- The system checks return codes for all operations, aborting safely on fatal errors.

## 6. Design Goals

- Simplicity in implementation.
- Clarity and separation of layers.
- Full traceability via verbose test output.
- Deterministic file layout for portability and debugging.
