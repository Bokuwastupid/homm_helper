#pragma once

#include <atomic>
#include <cstdint>

namespace arcanus {

struct LivePointersSnapshot {
    std::uintptr_t data = 0;
    std::uintptr_t transfer_battle = 0;
    std::uintptr_t selected_hero = 0;
};

class LivePointers {
public:
    void SetData(std::uintptr_t value) { data_.store(value, std::memory_order_release); }
    void SetTransferBattle(std::uintptr_t value) { transfer_battle_.store(value, std::memory_order_release); }
    void SetSelectedHero(std::uintptr_t value) { selected_hero_.store(value, std::memory_order_release); }

    LivePointersSnapshot Snapshot() const {
        return {
            data_.load(std::memory_order_acquire),
            transfer_battle_.load(std::memory_order_acquire),
            selected_hero_.load(std::memory_order_acquire)
        };
    }

private:
    std::atomic_uintptr_t data_{0};
    std::atomic_uintptr_t transfer_battle_{0};
    std::atomic_uintptr_t selected_hero_{0};
};

}

