#include <catch2/catch_test_macros.hpp>
#include <audio_loopback/audio_ring_buffer.h>
#include <thread>
#include <atomic>
#include <vector>

TEST_CASE("AudioRingBuffer basic operations", "[ring_buffer]") {
    audio::AudioRingBuffer<int, 16> buffer;
    
    SECTION("Empty buffer") {
        REQUIRE(buffer.empty());
        REQUIRE(!buffer.full());
        REQUIRE(buffer.available_read() == 0);
        REQUIRE(buffer.available_write() == 15); // One less than capacity
    }
    
    SECTION("Single item write and read") {
        REQUIRE(buffer.try_write(42));
        REQUIRE(!buffer.empty());
        REQUIRE(buffer.available_read() == 1);
        
        auto item = buffer.try_read();
        REQUIRE(item.is_some());
        REQUIRE(item.unwrap() == 42);
        REQUIRE(buffer.empty());
    }
    
    SECTION("Bulk operations") {
        int data[] = {1, 2, 3, 4, 5};
        size_t written = buffer.write_bulk(data, 5);
        REQUIRE(written == 5);
        REQUIRE(buffer.available_read() == 5);
        
        int read_data[5];
        size_t read = buffer.read_bulk(read_data, 5);
        REQUIRE(read == 5);
        for (int i = 0; i < 5; ++i) {
            REQUIRE(read_data[i] == i + 1);
        }
    }
    
    SECTION("Buffer full behavior") {
        // Fill buffer (capacity 16, but can only store 15)
        for (int i = 0; i < 15; ++i) {
            REQUIRE(buffer.try_write(i));
        }
        REQUIRE(buffer.full());
        REQUIRE(!buffer.try_write(99)); // Should fail
        
        // Read one to make space
        buffer.try_read();
        REQUIRE(!buffer.full());
        REQUIRE(buffer.try_write(99)); // Should succeed now
    }
    
    SECTION("Peek without consuming") {
        int data[] = {10, 20, 30, 40, 50};
        buffer.write_bulk(data, 5);
        
        int peek_data[3];
        size_t peeked = buffer.peek_bulk(peek_data, 3, 1); // Peek 3 items starting at offset 1
        REQUIRE(peeked == 3);
        REQUIRE(peek_data[0] == 20);
        REQUIRE(peek_data[1] == 30);
        REQUIRE(peek_data[2] == 40);
        
        // Buffer should still have all 5 items
        REQUIRE(buffer.available_read() == 5);
    }
}

TEST_CASE("AudioRingBuffer thread safety", "[ring_buffer][threading]") {
    audio::AudioRingBuffer<int, 1024> buffer;
    constexpr int num_items = 100000;
    std::atomic<bool> start{false};
    
    SECTION("Concurrent producer/consumer") {
        std::thread producer([&]() {
            while (!start.load()) {} // Wait for start
            
            for (int i = 0; i < num_items; ++i) {
                while (!buffer.try_write(i)) {
                    // Spin wait if buffer full
                    std::this_thread::yield();
                }
            }
        });
        
        std::thread consumer([&]() {
            std::vector<int> received;
            received.reserve(num_items);
            
            while (!start.load()) {} // Wait for start
            
            while (received.size() < num_items) {
                auto item = buffer.try_read();
                if (item.is_some()) {
                    received.push_back(item.unwrap());
                } else {
                    std::this_thread::yield();
                }
            }
            
            // Verify we received all items in order
            REQUIRE(received.size() == num_items);
            for (int i = 0; i < num_items; ++i) {
                REQUIRE(received[i] == i);
            }
        });
        
        start.store(true); // Start both threads
        
        producer.join();
        consumer.join();
    }
}

TEST_CASE("AudioRingBuffer wrap-around", "[ring_buffer]") {
    audio::AudioRingBuffer<int, 8> buffer; // Small buffer to test wrap-around
    
    SECTION("Write and read with wrap-around") {
        // Fill and empty multiple times
        for (int cycle = 0; cycle < 3; ++cycle) {
            // Write 6 items
            for (int i = 0; i < 6; ++i) {
                REQUIRE(buffer.try_write(cycle * 10 + i));
            }
            
            // Read them back
            for (int i = 0; i < 6; ++i) {
                auto item = buffer.try_read();
                REQUIRE(item.is_some());
                REQUIRE(item.unwrap() == cycle * 10 + i);
            }
        }
    }
}