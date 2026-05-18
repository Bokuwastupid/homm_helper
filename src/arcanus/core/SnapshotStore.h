#pragma once

#include <atomic>
#include <memory>

namespace arcanus {

template <typename T>
class SnapshotStore {
public:
    SnapshotStore()
        : front_(std::make_shared<T>()) {}

    std::shared_ptr<const T> Load() const {
        return front_.load(std::memory_order_acquire);
    }

    void Publish(T snapshot) {
        auto next = std::make_shared<T>(std::move(snapshot));
        front_.store(std::move(next), std::memory_order_release);
    }

private:
    mutable std::atomic<std::shared_ptr<T>> front_;
};

}
