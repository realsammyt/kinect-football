#pragma once

#include <array>
#include <mutex>
#include <atomic>

namespace kinect {
namespace core {

/**
 * @brief Thread-safe ring buffer for producer-consumer pattern
 *
 * Used to decouple capture thread from analysis thread.
 * From kinect-native: 30-frame capacity provides ~1 second buffer at 30fps.
 *
 * @tparam T Element type
 * @tparam Size Buffer capacity
 */
template<typename T, size_t Size>
class RingBuffer {
public:
    RingBuffer() = default;

    /**
     * @brief Push item to buffer (producer)
     * @param item Item to push
     * @return true if successful, false if buffer full
     */
    bool push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (count_ >= Size) {
            // Buffer full - drop oldest (overwrite)
            readIdx_ = (readIdx_ + 1) % Size;
            count_--;
        }

        buffer_[writeIdx_] = item;
        writeIdx_ = (writeIdx_ + 1) % Size;
        count_++;

        return true;
    }

    /**
     * @brief Pop item from buffer (consumer)
     * @param item Output item
     * @return true if item retrieved, false if buffer empty
     */
    bool pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (count_ == 0) {
            return false;
        }

        item = buffer_[readIdx_];
        readIdx_ = (readIdx_ + 1) % Size;
        count_--;

        return true;
    }

    /**
     * @brief Try to peek at front item without removing
     * @param item Output item
     * @return true if item available
     */
    bool peek(T& item) const {
        std::lock_guard<std::mutex> lock(mutex_);

        if (count_ == 0) {
            return false;
        }

        item = buffer_[readIdx_];
        return true;
    }

    /**
     * @brief Clear all items from buffer
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        readIdx_ = 0;
        writeIdx_ = 0;
        count_ = 0;
    }

    /**
     * @brief Get current number of items in buffer
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

    /**
     * @brief Check if buffer is empty
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_ == 0;
    }

    /**
     * @brief Check if buffer is full
     */
    bool full() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_ >= Size;
    }

    /**
     * @brief Get buffer capacity
     */
    constexpr size_t capacity() const {
        return Size;
    }

private:
    std::array<T, Size> buffer_;
    size_t readIdx_ = 0;
    size_t writeIdx_ = 0;
    size_t count_ = 0;
    mutable std::mutex mutex_;
};

} // namespace core
} // namespace kinect
