#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace hw4 {

inline constexpr std::uint32_t kQueueMagic = 0x51554555;
inline constexpr std::uint32_t kProtocolVersion = 2;
inline constexpr std::size_t kDefaultBufferSize = 4096;
inline constexpr std::uint32_t kWrapMarkerType = 0;

struct MessageHeader {
    std::uint32_t type = 0;
    std::uint32_t length = 0;
};

struct QueueHeader {
    std::uint32_t magic = kQueueMagic;
    std::uint32_t version = kProtocolVersion;
    std::uint32_t buffer_size = 0;
    std::uint32_t reserved = 0;

    std::atomic<std::uint64_t> write_reserve{0};
    std::atomic<std::uint64_t> write_commit{0};
    std::atomic<std::uint64_t> read_offset{0};
};

struct ReceivedMessage {
    std::uint32_t type = 0;
    std::vector<std::uint8_t> payload;
};

class ProducerNode {
public:
    ProducerNode(const std::string& shm_name, std::size_t buffer_size);
    ~ProducerNode();

    bool Send(std::uint32_t type, const void* data, std::size_t size);

    ProducerNode(const ProducerNode&) = delete;
    ProducerNode& operator=(const ProducerNode&) = delete;

private:
    std::string shm_name_;
    std::size_t mapped_size_ = 0;
    int fd_ = -1;
    void* mapping_ = nullptr;

    QueueHeader* header_ = nullptr;
    std::uint8_t* buffer_ = nullptr;

    void OpenOrCreate(std::size_t buffer_size);
    void Close();
};

class ConsumerNode {
public:
    explicit ConsumerNode(const std::string& shm_name);
    ~ConsumerNode();

    bool Receive(std::uint32_t desired_type, ReceivedMessage& out_message);

    ConsumerNode(const ConsumerNode&) = delete;
    ConsumerNode& operator=(const ConsumerNode&) = delete;

private:
    std::string shm_name_;
    std::size_t mapped_size_ = 0;
    int fd_ = -1;
    void* mapping_ = nullptr;

    QueueHeader* header_ = nullptr;
    std::uint8_t* buffer_ = nullptr;

    void OpenExisting();
    void Close();
};

} // namespace hw4