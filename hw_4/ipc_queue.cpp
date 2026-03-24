#include "ipc_queue.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace hw4 {
namespace {

std::size_t CalculateMappedSize(std::size_t buffer_size) {
    return sizeof(QueueHeader) + buffer_size;
}

void BindPointers(void* mapping, QueueHeader*& header, std::uint8_t*& buffer) {
    header = static_cast<QueueHeader*>(mapping);
    buffer = static_cast<std::uint8_t*>(mapping) + sizeof(QueueHeader);
}

std::runtime_error MakeError(const std::string& message) {
    return std::runtime_error(message + ": " + std::strerror(errno));
}

std::size_t MessageSize(std::size_t payload_size) {
    return sizeof(MessageHeader) + payload_size;
}

void WriteHeader(std::uint8_t* ptr, std::uint32_t type, std::uint32_t length) {
    MessageHeader header;
    header.type = type;
    header.length = length;
    std::memcpy(ptr, &header, sizeof(header));
}

MessageHeader ReadHeader(const std::uint8_t* ptr) {
    MessageHeader header{};
    std::memcpy(&header, ptr, sizeof(header));
    return header;
}

} // namespace

ProducerNode::ProducerNode(const std::string& shm_name, std::size_t buffer_size)
    : shm_name_(shm_name) {
    OpenOrCreate(buffer_size);
}

ProducerNode::~ProducerNode() {
    Close();
}

ConsumerNode::ConsumerNode(const std::string& shm_name)
    : shm_name_(shm_name) {
    OpenExisting();
}

ConsumerNode::~ConsumerNode() {
    Close();
}

void ProducerNode::OpenOrCreate(std::size_t buffer_size) {
    if (buffer_size <= sizeof(MessageHeader)) {
        throw std::runtime_error("buffer_size is too small");
    }

    mapped_size_ = CalculateMappedSize(buffer_size);

    shm_unlink(shm_name_.c_str());

    fd_ = shm_open(shm_name_.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd_ == -1) {
        throw MakeError("shm_open failed");
    }

    if (ftruncate(fd_, static_cast<off_t>(mapped_size_)) == -1) {
        Close();
        throw MakeError("ftruncate failed");
    }

    mapping_ = mmap(nullptr, mapped_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (mapping_ == MAP_FAILED) {
        mapping_ = nullptr;
        Close();
        throw MakeError("mmap failed");
    }

    BindPointers(mapping_, header_, buffer_);
    std::memset(mapping_, 0, mapped_size_);

    header_->magic = kQueueMagic;
    header_->version = kProtocolVersion;
    header_->buffer_size = static_cast<std::uint32_t>(buffer_size);
    header_->reserved = 0;
    header_->write_reserve.store(0, std::memory_order_relaxed);
    header_->write_commit.store(0, std::memory_order_relaxed);
    header_->read_offset.store(0, std::memory_order_relaxed);
}

void ConsumerNode::OpenExisting() {
    fd_ = shm_open(shm_name_.c_str(), O_RDWR, 0666);
    if (fd_ == -1) {
        throw MakeError("shm_open failed");
    }

    const std::size_t header_size = sizeof(QueueHeader);
    mapping_ = mmap(nullptr, header_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (mapping_ == MAP_FAILED) {
        mapping_ = nullptr;
        Close();
        throw MakeError("mmap header failed");
    }

    header_ = static_cast<QueueHeader*>(mapping_);

    if (header_->magic != kQueueMagic) {
        Close();
        throw std::runtime_error("invalid queue magic");
    }

    if (header_->version != kProtocolVersion) {
        Close();
        throw std::runtime_error("protocol version mismatch");
    }

    mapped_size_ = CalculateMappedSize(header_->buffer_size);

    if (munmap(mapping_, header_size) == -1) {
        Close();
        throw MakeError("munmap header failed");
    }

    mapping_ = mmap(nullptr, mapped_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (mapping_ == MAP_FAILED) {
        mapping_ = nullptr;
        Close();
        throw MakeError("mmap full queue failed");
    }

    BindPointers(mapping_, header_, buffer_);
}

void ProducerNode::Close() {
    if (mapping_ != nullptr) {
        munmap(mapping_, mapped_size_);
        mapping_ = nullptr;
    }

    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }

    header_ = nullptr;
    buffer_ = nullptr;
    mapped_size_ = 0;
}

void ConsumerNode::Close() {
    if (mapping_ != nullptr) {
        munmap(mapping_, mapped_size_);
        mapping_ = nullptr;
    }

    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }

    header_ = nullptr;
    buffer_ = nullptr;
    mapped_size_ = 0;
}

bool ProducerNode::Send(std::uint32_t type, const void* data, std::size_t size) {
    if (type == kWrapMarkerType) {
        return false;
    }

    if (data == nullptr && size != 0) {
        return false;
    }

    const std::size_t record_size = MessageSize(size);
    const std::size_t buffer_size = header_->buffer_size;

    if (record_size > buffer_size - sizeof(MessageHeader)) {
        return false;
    }

    std::uint64_t reserve_start = 0;
    std::uint64_t reserve_end = 0;
    bool need_wrap = false;

    while (true) {
        std::uint64_t current = header_->write_reserve.load(std::memory_order_relaxed);
        const std::size_t pos = static_cast<std::size_t>(current % buffer_size);

        need_wrap = (pos + record_size > buffer_size);
        const std::size_t padding = need_wrap ? (buffer_size - pos) : 0;
        const std::uint64_t next = current + padding + record_size;

        const std::uint64_t read = header_->read_offset.load(std::memory_order_acquire);
        if (next - read > buffer_size) {
            std::this_thread::yield();
            continue;
        }

        if (header_->write_reserve.compare_exchange_weak(
                current,
                next,
                std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
            reserve_start = current;
            reserve_end = next;
            break;
        }
    }

    std::size_t write_pos = static_cast<std::size_t>(reserve_start % buffer_size);

    if (need_wrap) {
        WriteHeader(buffer_ + write_pos, kWrapMarkerType, 0);
        write_pos = 0;
    }

    WriteHeader(buffer_ + write_pos, type, static_cast<std::uint32_t>(size));
    if (size != 0) {
        std::memcpy(buffer_ + write_pos + sizeof(MessageHeader), data, size);
    }

    while (header_->write_commit.load(std::memory_order_acquire) != reserve_start) {
        std::this_thread::yield();
    }

    header_->write_commit.store(reserve_end, std::memory_order_release);
    return true;
}

bool ConsumerNode::Receive(std::uint32_t desired_type, ReceivedMessage& out_message) {
    const std::size_t buffer_size = header_->buffer_size;

    while (true) {
        std::uint64_t read = header_->read_offset.load(std::memory_order_relaxed);
        const std::uint64_t committed = header_->write_commit.load(std::memory_order_acquire);

        if (committed <= read) {
            return false;
        }

        std::size_t pos = static_cast<std::size_t>(read % buffer_size);
        MessageHeader header = ReadHeader(buffer_ + pos);

        if (header.type == kWrapMarkerType && header.length == 0) {
            const std::uint64_t next = read + (buffer_size - pos);
            header_->read_offset.store(next, std::memory_order_release);
            continue;
        }

        const std::size_t record_size = MessageSize(header.length);
        if (read + record_size > committed) {
            return false;
        }

        if (header.type == desired_type) {
            out_message.type = header.type;
            out_message.payload.resize(header.length);

            if (header.length != 0) {
                std::memcpy(
                    out_message.payload.data(),
                    buffer_ + pos + sizeof(MessageHeader),
                    header.length
                );
            }
        }

        header_->read_offset.store(read + record_size, std::memory_order_release);
        return header.type == desired_type;
    }
}

} // namespace hw4