#pragma once
#include <atomic>
#include <array>
#include <cstddef>
#include <algorithm>
#include <new> // for hardware_destructive_interference_size
#include <core/option.h>

namespace audio {

// Lock-free single producer, single consumer ring buffer
// Optimized for audio streaming with minimal latency
template<typename T, size_t Capacity>
class AudioRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
private:
    // Cache line separation to avoid false sharing
    struct alignas(64) {
        std::atomic<size_t> write_pos{0};
    } writer_;
    
    struct alignas(64) {
        std::atomic<size_t> read_pos{0};
    } reader_;
    
    // The actual buffer
    alignas(64) std::array<T, Capacity> buffer_;
    
    static constexpr size_t mask_ = Capacity - 1;
    
public:
    AudioRingBuffer() = default;
    
    // Producer interface (audio thread)
    
    // Try to write a single item
    bool try_write(const T& item) {
        const size_t write = writer_.write_pos.load(std::memory_order::relaxed);
        const size_t next_write = (write + 1) & mask_;
        
        if (next_write == reader_.read_pos.load(std::memory_order::acquire)) {
            return false; // Buffer full
        }
        
        buffer_[write] = item;
        writer_.write_pos.store(next_write, std::memory_order::release);
        return true;
    }
    
    // Write multiple items, returns number written
    size_t write_bulk(const T* items, size_t count) {
        const size_t write = writer_.write_pos.load(std::memory_order::relaxed);
        const size_t read = reader_.read_pos.load(std::memory_order::acquire);
        
        const size_t available = (read - write - 1) & mask_;
        const size_t to_write = std::min(count, available);
        
        if (to_write == 0) return 0;
        
        // Handle wrap-around
        const size_t first_chunk = std::min(to_write, Capacity - write);
        std::copy_n(items, first_chunk, &buffer_[write]);
        
        if (first_chunk < to_write) {
            std::copy_n(items + first_chunk, to_write - first_chunk, &buffer_[0]);
        }
        
        writer_.write_pos.store((write + to_write) & mask_, std::memory_order::release);
        return to_write;
    }
    
    // Consumer interface (render thread)
    
    // Try to read a single item
    core::Option<T> try_read() {
        const size_t read = reader_.read_pos.load(std::memory_order::relaxed);
        
        if (read == writer_.write_pos.load(std::memory_order::acquire)) {
            return core::None<T>(); // Buffer empty
        }
        
        T item = buffer_[read];
        reader_.read_pos.store((read + 1) & mask_, std::memory_order::release);
        return core::Some(std::move(item));
    }
    
    // Read multiple items, returns number read
    size_t read_bulk(T* items, size_t count) {
        const size_t read = reader_.read_pos.load(std::memory_order::relaxed);
        const size_t write = writer_.write_pos.load(std::memory_order::acquire);
        
        const size_t available = (write - read) & mask_;
        const size_t to_read = std::min(count, available);
        
        if (to_read == 0) return 0;
        
        // Handle wrap-around
        const size_t first_chunk = std::min(to_read, Capacity - read);
        std::copy_n(&buffer_[read], first_chunk, items);
        
        if (first_chunk < to_read) {
            std::copy_n(&buffer_[0], to_read - first_chunk, items + first_chunk);
        }
        
        reader_.read_pos.store((read + to_read) & mask_, std::memory_order::release);
        return to_read;
    }
    
    // Peek at data without consuming (for visualization)
    size_t peek_bulk(T* items, size_t count, size_t offset = 0) const {
        const size_t read = reader_.read_pos.load(std::memory_order::relaxed);
        const size_t write = writer_.write_pos.load(std::memory_order::acquire);
        
        const size_t available = (write - read) & mask_;
        if (offset >= available) return 0;
        
        const size_t from_pos = (read + offset) & mask_;
        const size_t to_read = std::min(count, available - offset);
        
        if (to_read == 0) return 0;
        
        // Handle wrap-around
        const size_t first_chunk = std::min(to_read, Capacity - from_pos);
        std::copy_n(&buffer_[from_pos], first_chunk, items);
        
        if (first_chunk < to_read) {
            std::copy_n(&buffer_[0], to_read - first_chunk, items + first_chunk);
        }
        
        return to_read;
    }
    
    // Query functions
    size_t available_read() const {
        const size_t read = reader_.read_pos.load(std::memory_order::relaxed);
        const size_t write = writer_.write_pos.load(std::memory_order::acquire);
        return (write - read) & mask_;
    }
    
    size_t available_write() const {
        const size_t write = writer_.write_pos.load(std::memory_order::relaxed);
        const size_t read = reader_.read_pos.load(std::memory_order::acquire);
        return (read - write - 1) & mask_;
    }
    
    bool empty() const {
        return reader_.read_pos.load(std::memory_order::relaxed) == 
               writer_.write_pos.load(std::memory_order::acquire);
    }
    
    bool full() const {
        const size_t write = writer_.write_pos.load(std::memory_order::relaxed);
        const size_t next_write = (write + 1) & mask_;
        return next_write == reader_.read_pos.load(std::memory_order::acquire);
    }
    
    static constexpr size_t capacity() { return Capacity; }
};

} // namespace audio