#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

namespace {

struct Options {
    int port = 9006;
    fs::path root = "root";
};

Options parse_args(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            options.port = std::stoi(argv[++i]);
        } else if ((arg == "--root" || arg == "-r") && i + 1 < argc) {
            options.root = argv[++i];
        } else {
            std::cerr << "usage: " << argv[0] << " [--port PORT] [--root DIR]\n";
            std::exit(2);
        }
    }
    options.root = fs::absolute(options.root).lexically_normal();
    return options;
}

std::string reason_phrase(int status) {
    switch (status) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        default: return "Internal Server Error";
    }
}

std::string content_type(const fs::path& path) {
    const auto ext = path.extension().string();
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png") return "image/png";
    if (ext == ".gif") return "image/gif";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".mp4") return "video/mp4";
    return "application/octet-stream";
}

void write_all(int fd, const std::string& data) {
    const char* ptr = data.data();
    size_t remaining = data.size();
    while (remaining > 0) {
        ssize_t written = ::send(fd, ptr, remaining, 0);
        if (written <= 0) return;
        ptr += written;
        remaining -= static_cast<size_t>(written);
    }
}

void send_error(int client, int status) {
    const std::string body = "<html><body><h1>" + std::to_string(status) + " " + reason_phrase(status) + "</h1></body></html>";
    std::ostringstream response;
    response << "HTTP/1.1 " << status << ' ' << reason_phrase(status) << "\r\n"
             << "Content-Type: text/html; charset=utf-8\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << body;
    write_all(client, response.str());
}

bool path_within_root(const fs::path& root, const fs::path& target) {
    const auto rel = fs::relative(target, root);
    return rel.empty() || (rel.native().rfind("..", 0) != 0);
}

void handle_client(int client, fs::path root) {
    char buffer[4096]{};
    const ssize_t bytes = ::recv(client, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        ::close(client);
        return;
    }

    std::istringstream request(std::string(buffer, static_cast<size_t>(bytes)));
    std::string method;
    std::string uri;
    std::string version;
    request >> method >> uri >> version;

    if (method.empty() || uri.empty()) {
        send_error(client, 400);
    } else if (method != "GET" && method != "HEAD") {
        send_error(client, 405);
    } else {
        const auto query = uri.find('?');
        if (query != std::string::npos) uri.erase(query);
        if (uri == "/") uri = "/welcome.html";

        fs::path target = fs::weakly_canonical(root / uri.substr(1));
        if (!path_within_root(root, target)) {
            send_error(client, 403);
        } else if (!fs::is_regular_file(target)) {
            send_error(client, 404);
        } else {
            const auto size = fs::file_size(target);
            std::ostringstream header;
            header << "HTTP/1.1 200 OK\r\n"
                   << "Content-Type: " << content_type(target) << "\r\n"
                   << "Content-Length: " << size << "\r\n"
                   << "Connection: close\r\n\r\n";
            write_all(client, header.str());

            if (method == "GET") {
                int file = ::open(target.c_str(), O_RDONLY);
                if (file >= 0) {
                    off_t offset = 0;
                    while (offset < static_cast<off_t>(size)) {
                        const ssize_t sent = ::sendfile(client, file, &offset, size - static_cast<size_t>(offset));
                        if (sent <= 0) break;
                    }
                    ::close(file);
                }
            }
        }
    }

    ::close(client);
}

}  // namespace

int main(int argc, char** argv) {
    const Options options = parse_args(argc, argv);
    const int server = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        std::cerr << "socket: " << std::strerror(errno) << '\n';
        return 1;
    }

    int reuse = 1;
    ::setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(options.port));

    if (::bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 || ::listen(server, 128) < 0) {
        std::cerr << "bind/listen: " << std::strerror(errno) << '\n';
        ::close(server);
        return 1;
    }

    std::cout << "C++ TinyWebServer listening on http://127.0.0.1:" << options.port
              << "/ serving " << options.root << std::endl;

    while (true) {
        sockaddr_in peer{};
        socklen_t len = sizeof(peer);
        int client = ::accept(server, reinterpret_cast<sockaddr*>(&peer), &len);
        if (client < 0) continue;
        std::thread(handle_client, client, options.root).detach();
    }
}
