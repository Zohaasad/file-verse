# Omni File System (OFS) User Guide

This guide explains how to interact with the Omni File System as an end-user or administrator.

---

## 1. Accessing the Filesystem

### Initialize and Start
Before using the filesystem, ensure it has been formatted and initialized:


./ofs_test
The program will create a .omni container and set up the default admin user.
2. User Authentication
Login
Users must log in to perform filesystem operations. Example (admin):
SessionInfo* session;
user_login(instance, "admin", "admin_password", &session);
instance → pointer to filesystem instance
admin_password → password provided during formatting
session → active session object
Logout
user_logout(session);
3. File Operations
Create file: file_create(session, "/path/to/file.txt");
Read file: file_read(session, "/path/to/file.txt", &buffer, &size);
Edit file: file_edit(session, "/path/to/file.txt", data, size, index);
Delete file: file_delete(session, "/path/to/file.txt");
Truncate file: file_truncate(session, "/path/to/file.txt", new_size);
Rename file: file_rename(session, "/old_path.txt", "/new_path.txt");
Check existence: file_exists(session, "/path/to/file.txt");
Always free memory allocated by file_read() using free_buffer(buffer);
4. Directory Operations
Create directory: dir_create(session, "/my_directory");
List contents: dir_list(session, "/my_directory", &entries, &count);
Delete directory: dir_delete(session, "/my_directory");
Check existence: dir_exists(session, "/my_directory");
Directories must be empty before deletion.
5. Metadata and Permissions
Get metadata: get_metadata(session, "/path/to/file_or_dir", &meta);
Set permissions: set_permissions(session, "/path/to/file_or_dir", 0755);
Check filesystem stats: get_stats(session, &stats);
6. Admin Operations
Add user: user_add(instance, "username", "password", role);
Remove user: user_remove(instance, "username");
List users: user_list(instance, &users, &count);
Admin operations do not require an active session but need filesystem instance.
7. Shutting Down
Always cleanly shut down the filesystem to persist all changes:
fs_shutdown(instance);
