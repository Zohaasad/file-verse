# OFS – Omni File Server: Design Choices

This document explains the core architecture, design rationale, and implementation strategies behind the Omni File Server (OFS). It covers the system's internal structure, request processing, concurrency model, virtual filesystem design, and data handling strategies.

## Table of Contents

1. [Overall Architecture](#overall-architecture)
2. [Modules and Responsibilities](#modules-and-responsibilities)
3. [Data Structures Layer](#data-structures-layer)
4. [Core Layer](#core-layer)
5. [Interface Layer](#interface-layer)
6. [Client-Server Communication](#client-server-communication)
7. [Session and Authentication](#session-and-authentication)
8. [Virtual Filesystem Design](#virtual-filesystem-design)
9. [Concurrency and Threading Model](#concurrency-and-threading-model)
10. [Memory and Performance](#memory-and-performance)
11. [Security and Robustness](#security-and-robustness)
12. [Design Goals](#design-goals)

---

## Overall Architecture

OFS is designed as a lightweight, modular virtual filesystem, implemented in C++. It simulates a persistent, block-based filesystem stored inside a single `.omni` binary file.

### Layered Structure

```
+-----------------+ TCP +-----------------+
| Terminal Client | <------------------> | OFS Server |
+-----------------+ +-----------------+
| Request Queue   |
| Worker Threads  |
| Session Manager |
| Virtual FS      |
+-----------------+
```

- **Server** handles all filesystem operations, authentication, and session management.
- **Client** handles user input, display, and command formatting.
- **Communication** is over TCP, with newline-delimited commands and JSON responses.

---

## Modules and Responsibilities

### 1. Server Core (`server.cpp`, `server.hpp`)

- Accepts TCP connections.
- Manages client sessions (`client_fd → session`).
- Queues client requests in a thread-safe `TSQueue<OFSRequest>`.
- Spawns worker threads to process requests concurrently.
- Sends JSON-formatted responses to clients.

### 2. File System Core (`ofs_core.hpp` + `ofs_core.cpp`)

- `ofs_core.hpp` declares the virtual filesystem API.
- `ofs_core.cpp` implements filesystem logic, including reading/writing file data, managing metadata, and maintaining internal structures (`FileEntry`, `FileMetadata`).
- Provides operations: create/read/edit/delete files and directories, rename, truncate.
- Maintains metadata: owner, permissions, timestamps, block usage.
- Uses a precompiled `.omni` file as the FS image; runtime operations manipulate the in-memory structures.

### 3. Client (`ofs_terminal_ui.py`)

- Terminal-based, menu-driven interface using curses.
- Supports input boxes, read/edit file screens, and scrollable file views.
- Sends user commands to the server and interprets JSON responses.

---

## Data Structures Layer

### SimpleHashMap

- A templated hash map implemented from scratch to avoid STL overhead.
- Used for fast lookup of user indices, path mappings, and session references.
- Key operations: `insert`, `erase`, `contains`, `find` – average O(1) complexity.

### MetaEntry

- Represents a file or directory record.
- Each entry is fixed-size (72 bytes), ensuring predictable on-disk alignment.
- Fields include permissions, timestamps, and owner ID for Unix-like semantics.

### FSInstance

- Holds all runtime state:
  - File handle for the `.omni` file.
  - Metadata vectors.
  - Bitmaps for free space tracking.
  - Session and user maps.
  - Encoding and security buffers.
- Encapsulation ensures thread-safe multi-session handling.

---

## Core Layer

- Implements all filesystem operations and internal state management.
- Uses the data structures layer for indexing and caching.
- Supports atomic operations for files and directories.
- Provides error codes (`OFSErrorCodes`) for all operations.
- Separates low-level storage logic from networking and UI.

---

## Interface Layer

- Exposes user-facing APIs: `fs_format`, `file_create`, `user_login`, etc.
- Provides a clean boundary for the server to invoke filesystem operations safely.
- Functions are designed to be modular and testable independently.

---

## Client-Server Communication

**Protocol:** TCP, text-based commands with newline separators.

**Command Format:** `operation arg1 arg2 ...`

**Response Format:** JSON, e.g.:

```json
{
  "status": "success",
  "operation": "read_file",
  "request_id": "0",
  "error_message": "",
  "data": "File content here"
}
```

- Server parses quoted arguments to support filenames with spaces.
- **Advantages:** human-readable, structured, easy to debug.

---

## Session and Authentication

- Each connection gets a session object, stored in a thread-safe map (`client_fd → session`).
- **Roles:** `admin` (full access), `normal` (restricted).
- **Lifecycle:**
  - Login → session created.
  - Requests → session validated.
  - Logout → session removed.
- Unauthorized operations return `ERROR_INVALID_SESSION`.

---

## Virtual Filesystem Design

- `.omni` file stores the persistent filesystem image.
- Supports hierarchical directories and files.
- **File operations:** create, read, edit, delete, rename, truncate.
- **Directory operations:** create, list, delete.
- **Metadata:** owner, permissions, timestamps, block usage.
- Internal structures (`FileEntry`, `FileMetadata`) ensure atomic, thread-safe operations.

---

## Concurrency and Threading Model

- **Accept Thread:** handles new client connections.
- **Worker Threads:** process requests from `TSQueue<OFSRequest>`.
- **Thread-Safe Queue (`TSQueue`):** ensures safe request queuing.
- **Session Mutex:** protects `client_sessions` from concurrent access.
- **Rationale:** separates network I/O from filesystem operations, allowing multiple concurrent clients without blocking.

---

## Memory and Performance

- Hash-based caching reduces repeated lookups.
- Minimal heap usage: vectors and static buffers avoid unnecessary allocations.
- Fixed-size metadata blocks guarantee predictable on-disk layout and fast seeks.
- Efficient memory management ensures performance even with many concurrent clients.

---

## Security and Robustness

- Private keys and encoding maps are pre-zeroed to prevent uninitialized memory leaks.
- User credentials enforce access control.
- All functions return codes, aborting safely on fatal errors.
- Modular design allows replacing or extending components safely.

---

## Design Goals

- **Simplicity and clarity** in implementation.
- **Separation of layers** (data structures, core, interface, client).
- **Full traceability** via verbose test output.
- **Deterministic file layout** for portability and debugging.
- **Robust multi-client support** with consistent error handling.

---

## Summary

OFS is designed for robustness, scalability, and maintainability:

- **Modular architecture** separates networking, sessions, and filesystem management.
- **Threaded request processing** ensures responsive multi-client performance.
- **JSON-based communication** simplifies client implementation and debugging.
- **Terminal client** provides a user-friendly interface without GUI dependencies.
- **Detailed core data structures** and memory strategies ensure fast, safe, and traceable operations.
