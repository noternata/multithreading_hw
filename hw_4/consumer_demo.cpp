#include "ipc_queue.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

int main() {
    try {
        const std::string shm_name = "/hw4_demo_queue";
        const std::uint32_t desired_type = 1;
        const int target_messages = 3;

        hw4::ConsumerNode consumer(shm_name);

        std::cout << "Consumer started.\n";
        std::cout << "Reading messages of type " << desired_type << " from "
                  << shm_name << '\n';

        int received_count = 0;

        while (received_count < target_messages) {
            hw4::ReceivedMessage message;

            if (consumer.Receive(desired_type, message)) {
                std::string text(message.payload.begin(), message.payload.end());

                std::cout << "Received message: ";
                std::cout << "type=" << message.type
                          << ", length=" << message.payload.size()
                          << ", text=\"" << text << "\"\n";

                ++received_count;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        std::cout << "Consumer finished after receiving "
                  << received_count << " matching messages.\n";
    } catch (const std::exception& ex) {
        std::cerr << "Consumer error: " << ex.what() << '\n';
        return 1;
    }
}