OFS – Omni File Server: Design Choices
This document explains the design rationale, architecture, and implementation strategies of the Omni File Server (OFS). It details the data structures, virtual filesystem layout, concurrency model, memory strategies, and reasoning behind each choice.
Table of Contents
Overall Architecture
Data Structures and Rationale
User Indexing
Directory Tree Representation
Free Space Tracking
Mapping File Paths to Disk
.omni File Structure
Memory Management
Optimizations
Summary
Overall Architecture
OFS is a modular, multi-user, TCP-based virtual filesystem, implemented in C++. The system is layered:
+-----------------+ TCP +-----------------+
| Terminal Client | <------------------> | OFS Server |
+-----------------+ +-----------------+
| Request Queue   |
| Worker Threads  |
| Session Manager |
| Virtual FS      |
+-----------------+
Server: Handles authentication, session management, and filesystem operations.
Client: Terminal-based menu UI for user commands.
Communication: TCP with newline-delimited commands and JSON responses.
Reasoning:
Layer separation improves maintainability and testability.
Multi-threaded design allows multiple clients simultaneously without blocking.
Queue-based request processing decouples network I/O from filesystem operations for better performance.
Data Structures and Rationale
Hash Map (SimpleHashMap)
Used for user indexing, path-to-metadata lookup, and session management.
Reason: Provides O(1) average lookup, crucial for fast user_login and file access.
Thread-Safe Queue (TSQueue)
Stores incoming client requests before worker threads process them.
Reason: Ensures thread-safe producer-consumer behavior, allowing concurrent clients without race conditions.
Vectors / Fixed-size Metadata
Metadata and session lists are stored in vectors of fixed-size entries.
Reason: Predictable memory layout and fast iteration/random access.
User Indexing
Structure: SimpleHashMap<std::string, UserInfo>
Maps username → UserInfo (password, role, session).
Reasoning:
user_login requires instant lookup by username.
Hash map avoids scanning a list of users.
Supports O(1) insertion, deletion, and verification.
Directory Tree Representation
Structure: MetaEntry nodes for files/directories stored in a metadata vector.
Each node contains:
Name, type (file/dir)
Permissions and owner
Parent/child references
Reasoning:
Allows fast hierarchical traversal.
Supports directory creation, deletion, and listing efficiently.
Easy to serialize/deserialize to the .omni file.
Free Space Tracking
Structure: Bitmap (one bit per block).
Reasoning:
Efficient O(1) allocation/deallocation of blocks.
Minimizes overhead for large filesystems.
Easy to persist in the .omni file header.
Mapping File Paths to Disk
Structure: FileMetadata per file, storing path, size, and block pointers.
Path Index: Maps full path → metadata entry.
Reasoning:
Avoids scanning the entire disk to locate a file.
Supports fast reads, edits, and truncation.
Enables atomic updates without corrupting other files.
.omni File Structure
Section	Purpose
Header (OMNIHeader)	Basic config, offsets, and bitmap location
Bitmap	Tracks free/used data blocks
Metadata Region	Stores MetaEntry records (fixed-size)
Data Blocks	Actual file content
Reasoning:
Deterministic layout enables fast seeks and predictable memory usage.
Fixed-size metadata entries simplify indexing and serialization.
Separates metadata from data for atomic updates and concurrency safety.
Memory Management
Dynamically allocated buffers for file content.
Temporary buffers used during edits for atomic writes.
Fixed-size entries and pre-allocated vectors reduce heap fragmentation.
Clients must call free_buffer() to release memory after reading.
Reasoning:
Ensures safety, avoids memory leaks, and supports multi-client concurrency.
Optimizations
Hash-based caching reduces repeated lookups.
Fixed-size metadata ensures predictable on-disk offsets.
Thread-safe queue decouples network and FS processing for performance.
Atomic file edits prevent partial writes.
Multi-threaded worker pool improves responsiveness under load.
Summary
Hash maps: O(1) lookups for users and paths.
Bitmaps: Efficient free block tracking.
MetaEntry nodes: Structured directory and file storage.
TSQueue: Thread-safe request handling.
.omni layout: Deterministic and atomic-friendly.
Memory strategies: Temporary buffers, fixed-size entries, minimal heap usage.
Each design choice balances performance, safety, and concurrency to make OFS robust for multi-user environments.
Operation Queue & Thread Management
OFS uses a thread-safe request queue (TSQueue) to manage client operations efficiently. The server architecture separates networking from processing, allowing multiple clients to interact concurrently without blocking.
How it works:
Accept Thread: Continuously listens for new client connections over TCP. Each new client is assigned a session and a file descriptor.
Request Queue:
Incoming client commands are parsed and wrapped as OFSRequest objects.
Requests are pushed into TSQueue, a thread-safe queue with internal mutex and condition variable.
If the queue is empty, worker threads wait efficiently (blocking) until a new request arrives.
Worker Threads:
One or more threads continuously pop requests from the queue.
Each worker thread processes the request: validates the session, executes the requested filesystem operation, and generates a JSON-formatted response.
Response Handling:
Responses (OFSResponse) contain the client file descriptor and JSON data.
The server sends responses back over TCP to the corresponding client socket.
Thread-safe mechanisms ensure responses are sent correctly even under high concurrency.
Key Benefits:
Concurrency: Multiple clients can perform operations simultaneously.
Thread Safety: Mutexes and atomic operations prevent race conditions.
Responsiveness: Accept thread handles connections without blocking file operations.
Scalability: Worker threads can be increased to handle higher loads efficiently.
