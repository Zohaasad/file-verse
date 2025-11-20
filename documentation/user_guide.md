# OFS – Omni File Server: User Guide

This guide provides step-by-step instructions for interacting with the Omni File Server (OFS) as an end-user or administrator. It covers installation, client usage, file and directory operations, user management, session handling, advanced features, and best practices.

## Table of Contents

1. [Getting Started](#getting-started)
2. [Launching the Client](#launching-the-client)
3. [Login & Authentication](#login--authentication)
4. [File Operations](#file-operations)
5. [Directory Operations](#directory-operations)
6. [User Management (Admin Only)](#user-management-admin-only)
7. [Session Management](#session-management)
8. [Advanced Features](#advanced-features)
9. [Tips & Best Practices](#tips--best-practices)
10. [Shutting Down](#shutting-down)

---

## Getting Started

Before using OFS, ensure that:

- The filesystem has been initialized and formatted.
- Required configuration files exist:

```
compiled/sample.omni
compiled/default.uconf
```

### Initializing and Starting the Filesystem

To create a new `.omni` container and set up the default admin user:

```bash
./ofs_test
```

- The program creates a `.omni` container.
- Sets up the default `admin` user.
- Prepares the filesystem for use.

---

## Launching the Client

Navigate to your project directory and run the client:

```bash
cd ~/Documents/fileverse_project
python3 source/ui/ofs_terminal_ui.py
```

- Connects to the server at `localhost:8080` by default.
- To connect to a remote server or a different port, modify the `SERVER_HOST` and `SERVER_PORT` variables in `ofs_terminal_ui.py`.

---

## Login & Authentication

### 1. Login Prompt

- Enter your username and password.
- Users have one of two roles:
  - `admin`: Full access, including user management.
  - `normal`: Limited access; cannot manage other users.

### 2. Failed Login

- Displays an error message.
- Leaving username and password empty exits the client.

### Example API Usage

```cpp
SessionInfo* session;
user_login(instance, "admin", "admin_password", &session);
```

- `instance`: pointer to filesystem instance
- `admin_password`: password for the admin user
- `session`: pointer to active session object

---

## File Operations

### 1. Create File

```cpp
file_create(session, "/path/to/file.txt");
```

- Allocates metadata and blocks.
- Writes content sequentially.
- Updates path indices.

### 2. Read File

```cpp
file_read(session, "/path/to/file.txt", &buffer, &size);
```

- Returns file content in a dynamically allocated buffer.
- **Important:** Always free memory after reading:

```cpp
free_buffer(buffer);
```

### 3. Edit File

```cpp
file_edit(session, "/path/to/file.txt", data, size, offset);
```

- Allows partial updates without rewriting the entire file.
- Locks file during edits to ensure atomic operation.

### 4. Truncate File

```cpp
file_truncate(session, "/path/to/file.txt", new_size);
```

- **Shrinking:** frees trailing blocks.
- **Expanding:** allocates zero-filled blocks.

### 5. Rename File

```cpp
file_rename(session, "/old_path.txt", "/new_path.txt");
```

- Updates metadata and path indices atomically.

### 6. Delete File

```cpp
file_delete(session, "/path/to/file.txt");
```

- Frees memory and updates bitmap.
- Validates permissions before deletion.

### 7. Check File Existence

```cpp
file_exists(session, "/path/to/file.txt");
```

- Returns success if file exists.

---

## Directory Operations

### 1. Create Directory

```cpp
dir_create(session, "/my_directory");
```

### 2. Delete Directory

```cpp
dir_delete(session, "/my_directory");
```

- Directory must be empty.

### 3. List Contents

```cpp
dir_list(session, "/my_directory", &entries, &count);
```

### 4. Check Directory Existence

```cpp
dir_exists(session, "/my_directory");
```

- Returns success if directory exists.

---

## User Management (Admin Only)

### 1. Add User

```cpp
user_add(instance, "username", "password", role);
```

- `role` = `admin` or `normal`.

### 2. Remove User

```cpp
user_remove(instance, "username");
```

### 3. List Users

```cpp
user_list(instance, &users, &count);
```

- Admin operations require the filesystem instance, not an active session.

---

## Session Management

- Tracks connected users and operations.
- Navigate to **Session Info** in the terminal client to view:
  - Session ID
  - User and role
  - Login time
  - Last activity
  - Operation count
- **Logout:** Ends the session safely and frees allocated buffers:

```cpp
user_logout(session);
```

---

## Advanced Features

### 1. Set Permissions

```cpp
set_permissions(session, "/path/to/file_or_dir", 0755);
```

- Only `admin` users can modify permissions.

### 2. View Metadata

```cpp
get_metadata(session, "/path/to/file_or_dir", &meta);
```

- Shows type, size, owner, permissions, timestamps, blocks used.

### 3. Filesystem Statistics

```cpp
get_stats(session, &stats);
```

- Returns free space, file count, and directory count.

---

## Tips & Best Practices

- Save work before exiting the editor.
- Use JSON responses for debugging.
- Admins should regularly monitor sessions and permissions.
- Free buffers after reading files (`free_buffer()`).
- Avoid very long lines; terminal wraps content automatically.
- Large files may scroll slowly; consider splitting.

---

## Shutting Down

Always perform a clean shutdown to persist changes:

```cpp
fs_shutdown(instance);
```

- Ensures all metadata and data blocks are flushed to the `.omni` file.
