
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>
#include <cstdlib>
#include "../core/ofs_instance.hpp"
#include "../core/ofs_core.hpp"
#include "../include/ofs_types.hpp"
#include "../core/meta_entry.hpp"
#include "server_network.hpp"

#define PORT 8080
#define BUFFER_SIZE 4096

void handle_client(int client_socket, void* fs_instance) {
    char buffer[BUFFER_SIZE];
    void* session = nullptr;

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = read(client_socket, buffer, BUFFER_SIZE - 1);
        if (bytes <= 0) break; 
        std::string req_str(buffer);
        ClientRequest req = parse_request(req_str);

        ServerResponse res{};
        try {
            if (req.command == "login" && req.args.size() == 2) {
                int r = user_login(&session, req.args[0].c_str(), req.args[1].c_str());
                res.success = (r == 0);
                res.message = get_error_message(r);
            } 
            else if (req.command == "logout") {
                int r = user_logout(session);
                res.success = (r == 0);
                res.message = get_error_message(r);
                session = nullptr;
            } 
            else if (req.command == "create_user" && req.args.size() == 3) {
                UserRole role = (req.args[2] == "admin") ? UserRole::ADMIN : UserRole::NORMAL;
                int r = user_create(session, req.args[0].c_str(), req.args[1].c_str(), role);
                res.success = (r == 0);
                res.message = get_error_message(r);
            } 
            else if (req.command == "delete_user" && req.args.size() == 1) {
                int r = user_delete(session, req.args[0].c_str());
                res.success = (r == 0);
                res.message = get_error_message(r);
            } 
            else if (req.command == "list_users") {
                UserInfo* users = nullptr;
                int count = 0;
                int r = user_list(session, &users, &count);
                res.success = (r == 0);
                res.message = get_error_message(r);
                if (r == 0 && users) {
                    std::string data;
                    for (int i = 0; i < count; ++i) {
                        data += std::string(users[i].username) + (users[i].is_active ? " (active)" : " (inactive)") + "\n";
                    }
                    res.data = data;
                    free_buffer(users);
                }
            } 
            else if (req.command == "create_dir" && req.args.size() == 1) {
                int r = dir_create(session, req.args[0].c_str());
                res.success = (r == 0);
                res.message = get_error_message(r);
            } 
            else if (req.command == "delete_dir" && req.args.size() == 1) {
                int r = dir_delete(session, req.args[0].c_str());
                res.success = (r == 0);
                res.message = get_error_message(r);
            } 
            else if (req.command == "create_file" && req.args.size() >= 2) {
                std::string path = req.args[0];
                std::string content = req.args[1];
                int r = file_create(session, path.c_str(), content.c_str(), content.size());
                res.success = (r == 0);
                res.message = get_error_message(r);
            } 
            else if (req.command == "read_file" && req.args.size() == 1) {
                char* buf = nullptr;
                size_t sz = 0;
                int r = file_read(session, req.args[0].c_str(), &buf, &sz);
                res.success = (r == 0);
                res.message = get_error_message(r);
                if (buf) {
                    res.data.assign(buf, sz);
                    free_buffer(buf);
                }
            } 
            else if (req.command == "edit_file" && req.args.size() == 3) {
                const char* path = req.args[0].c_str();
                const char* text = req.args[1].c_str();
                size_t index = std::stoul(req.args[2]);
                int r = file_edit(session, path, text, strlen(text), index);
                res.success = (r == 0);
                res.message = get_error_message(r);
            } 
            else if (req.command == "truncate_file" && req.args.size() == 2) {
                const char* path = req.args[0].c_str();
                size_t new_size = std::stoul(req.args[1]);
                int r = file_truncate(session, path, new_size);
                res.success = (r == 0);
                res.message = get_error_message(r);
            } 
            else if (req.command == "rename_file" && req.args.size() == 2) {
                int r = file_rename(session, req.args[0].c_str(), req.args[1].c_str());
                res.success = (r == 0);
                res.message = get_error_message(r);
            } 
            else if (req.command == "delete_file" && req.args.size() == 1) {
                int r = file_delete(session, req.args[0].c_str());
                res.success = (r == 0);
                res.message = get_error_message(r);
            } 
            else {
                res.success = false;
                res.message = "Unknown command or wrong arguments";
            }
        } catch (...) {
            res.success = false;
            res.message = "Internal server error";
        }

        std::string resp_str = serialize_response(res);
        send(client_socket, resp_str.c_str(), resp_str.size(), 0);
        std::cout << "Processed request: " << req_str << " -> " << res.message << "\n";
    }

    if (session) user_logout(session);
    close(client_socket);
    std::cout << "Client disconnected.\n";
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) { perror("socket failed"); exit(EXIT_FAILURE); }

    sockaddr_in address{};
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) { perror("listen"); exit(EXIT_FAILURE); }

    std::cout << "[OFS] Server started, listening on port " << PORT << "\n";

    void* fs_instance = nullptr;
    int r = fs_init(&fs_instance, "compiled/sample.omni", "compiled/default.uconf");
    if (r != 0) {
        std::cerr << "Failed to init filesystem: " << get_error_message(r) << "\n";
        exit(EXIT_FAILURE);
    }

    while (true) {
        int client_socket = accept(server_fd, nullptr, nullptr);
        if (client_socket >= 0) {
            std::thread(handle_client, client_socket, fs_instance).detach();
        }
    }

    fs_shutdown(fs_instance);
    close(server_fd);
    return 0;
}
