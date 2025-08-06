// Example UDP listener for Shards Debug Adapter service discovery on port 57426
// Compile with: g++ -std=c++17 discovery_example.cpp -lboost_system -pthread -o discovery_listener

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>

using json = nlohmann::json;
using boost::asio::ip::udp;

class DiscoveryListener {
private:
    boost::asio::io_context io_context_;
    udp::socket socket_;
    std::array<char, 1024> recv_buffer_;

public:
    DiscoveryListener() : socket_(io_context_, udp::endpoint(udp::v4(), 57426)) {
        socket_.set_option(boost::asio::socket_base::reuse_address(true));
    }

    void start_listen() {
        start_receive();
        std::cout << "Listening for Shards Debug Adapter announcements on port 57426..." << std::endl;
        io_context_.run();
    }

private:
    void start_receive() {
        socket_.async_receive_from(
            boost::asio::buffer(recv_buffer_), remote_endpoint_,
            [this](boost::system::error_code ec, std::size_t bytes_recvd) {
                if (!ec && bytes_recvd > 0) {
                    handle_receive(bytes_recvd);
                }
                start_receive();
            });
    }

    void handle_receive(std::size_t bytes_recvd) {
        try {
            std::string message(recv_buffer_.data(), bytes_recvd);
            json announcement = json::parse(message);
            
            if (announcement["service"] == "shards-debug-adapter") {
                std::cout << "\n=== Shards Debug Adapter Found ===" << std::endl;
                std::cout << "Instance: " << announcement["instance"]["name"] << std::endl;
                std::cout << "Port: " << announcement["instance"]["port"] << std::endl;
                std::cout << "Protocol: " << announcement["instance"]["protocol"] << std::endl;
                std::cout << "Version: " << announcement["version"] << std::endl;
                std::cout << "From: " << remote_endpoint_.address() << std::endl;
                
                auto caps = announcement["capabilities"];
                std::cout << "Capabilities:" << std::endl;
                for (auto& [key, value] : caps.items()) {
                    if (value.is_boolean() && value.get<bool>()) {
                        std::cout << "  - " << key << std::endl;
                    }
                }
                std::cout << "================================" << std::endl;
            }
        } catch (const std::exception& e) {
            // Ignore invalid JSON or non-Shards messages
        }
    }

    udp::endpoint remote_endpoint_;
};

int main() {
    try {
        DiscoveryListener listener;
        listener.start_listen();
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}