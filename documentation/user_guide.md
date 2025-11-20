OFS – Omni File Server: User Guide
This guide provides step-by-step instructions for interacting with the Omni File Server (OFS) as an end-user or administrator. It covers installation, client usage, file and directory operations, user management, session handling, advanced features, and best practices.
Table of Contents
Getting Started
Building and Launching the Server
Launching the Client
Login & Authentication
File Operations
Directory Operations
User Management (Admin Only)
Session Management
Advanced Features
Tips & Best Practices
Shutting Down
Getting Started
Before using OFS, ensure that:
The filesystem has been initialized and formatted.
Required configuration files exist:
compiled/sample.omni
compiled/default.uconf
Initializing and Starting the Filesystem
To create a new .omni container and set up the default admin user:
cd ~/Documents/fileverse_project
./ofs_test
Creates a .omni container.
Sets up the default admin user.
Prepares the filesystem for use.
Building and Launching the Server
From the project root (~/Documents/fileverse_project), compile the server:
g++ -std=c++17 -pthread \
-I./source/include -I./source/core -I./source/data_structures \
source/server/main_server.cpp \
source/server/server.cpp \
source/core/ofs_core.cpp \
-o compiled/server
This creates the server executable at compiled/server.
To start the server:
cd ~/Documents/fileverse_project
./compiled/server
Server listens on the default port (8080).
Configuration options (port, max connections, timeout) are set in compiled/default.uconf.
Launching the Client
From the project root:
cd ~/Documents/fileverse_project
python3 source/ui/ofs_terminal_ui.py
Connects to the server at localhost:8080 by default.
To connect to a different host or port, modify SERVER_HOST and SERVER_PORT in ofs_terminal_ui.py.
Login & Authentication
1. Login Prompt
Enter your username and password.
Users have one of two roles:
admin: Full access, including user management.
normal: Limited access; cannot manage other users.
2. Failed Login
Displays an error message.
Leaving username and password empty exits the client.
Example API Usage
SessionInfo* session;
user_login(instance, "admin", "admin_password", &session);
instance: pointer to filesystem instance
admin_password: password for the admin user
session: pointer to active session object
File Operations
1. Create File
file_create(session, "/path/to/file.txt");
Allocates metadata and blocks.
Writes content sequentially.
Updates path indices.
2. Read File
file_read(session, "/path/to/file.txt", &buffer, &size);
Returns file content in a dynamically allocated buffer.
Important: Free memory after reading:
free_buffer(buffer);
3. Edit File
file_edit(session, "/path/to/file.txt", data, size, offset);
Allows partial updates without rewriting the entire file.
Locks file during edits to ensure atomic operation.
4. Truncate File
file_truncate(session, "/path/to/file.txt", new_size);
Shrinking: frees trailing blocks.
Expanding: allocates zero-filled blocks.
5. Rename File
file_rename(session, "/old_path.txt", "/new_path.txt");
Updates metadata and path indices atomically.
6. Delete File
file_delete(session, "/path/to/file.txt");
Frees memory and updates bitmap.
Validates permissions before deletion.
7. Check File Existence
file_exists(session, "/path/to/file.txt");
Returns success if file exists.
Directory Operations
1. Create Directory
dir_create(session, "/my_directory");
2. Delete Directory
dir_delete(session, "/my_directory");
Directory must be empty.
3. List Contents
dir_list(session, "/my_directory", &entries, &count);
4. Check Directory Existence
dir_exists(session, "/my_directory");
Returns success if directory exists.
User Management (Admin Only)
1. Add User
user_add(instance, "username", "password", role);
role = admin or normal.
2. Remove User
user_remove(instance, "username");
3. List Users
user_list(instance, &users, &count);
Admin operations require the filesystem instance, not an active session.
Session Management
Tracks connected users and operations.
Navigate to Session Info in the terminal client to view:
Session ID
User and role
Login time
Last activity
Operation count
Logout: Ends the session safely:
user_logout(session);
Advanced Features
1. Set Permissions
set_permissions(session, "/path/to/file_or_dir", 0755);
Only admin users can modify permissions.
2. View Metadata
get_metadata(session, "/path/to/file_or_dir", &meta);
Shows type, size, owner, permissions, timestamps, blocks used.
3. Filesystem Statistics
get_stats(session, &stats);
Returns free space, file count, and directory count.
Tips & Best Practices
Save work before exiting the editor.
Use JSON responses for debugging.
Admins should regularly monitor sessions and permissions.
Free buffers after reading files (free_buffer()).
Avoid very long lines; terminal wraps content automatically.
Large files may scroll slowly; consider splitting.
Shutting Down
Always perform a clean shutdown to persist changes:
fs_shutdown(instance);
Ensures all metadata and data blocks are flushed to the .omni file.
