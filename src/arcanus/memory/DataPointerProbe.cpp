#include "arcanus/memory/DataPointerProbe.h"

#include "arcanus/memory/KnownOffsets.h"
#include "arcanus/memory/SafeMemory.h"

#ifdef ARCANUS_WITH_DX11_HOOK
#include <MinHook.h>
#endif

namespace arcanus {
namespace {

using GenericDataThisFn = void(__fastcall*)(void* self, void* a2, void* a3, void* a4);
using DataMapFn = void(__fastcall*)(void* self, void* map_data);
using WorldCameraUpdateFn = void(__fastcall*)(void* self, void* method);
using WorldCameraInitFn = void(__fastcall*)(void* self, void* map, bool is_in_editor, void* method);

std::atomic_uintptr_t g_data_ptr{0};
std::atomic_uintptr_t g_map_data_ptr{0};
std::atomic_uintptr_t g_world_camera_ptr{0};
std::atomic_uintptr_t g_unity_camera_ptr{0};
std::atomic_uintptr_t g_world_map_ptr{0};
std::atomic_bool g_installed{false};
GenericDataThisFn g_original_data_init = nullptr;
DataMapFn g_original_data_load_map = nullptr;
WorldCameraUpdateFn g_original_world_camera_update = nullptr;
WorldCameraInitFn g_original_world_camera_init = nullptr;
std::uintptr_t g_data_init_hook_address = 0;
std::uintptr_t g_data_load_map_hook_address = 0;
std::uintptr_t g_world_camera_update_hook_address = 0;
std::uintptr_t g_world_camera_init_hook_address = 0;

void __fastcall DataInitHook(void* self, void* a2, void* a3, void* a4) {
    DataPointerProbe::StoreDataPointer(self, a2);
    if (g_original_data_init != nullptr) {
        g_original_data_init(self, a2, a3, a4);
    }
}

void __fastcall DataLoadMapHook(void* self, void* map_data) {
    DataPointerProbe::StoreDataPointer(self, nullptr);
    DataPointerProbe::StoreMapDataPointer(map_data);
    if (g_original_data_load_map != nullptr) {
        g_original_data_load_map(self, map_data);
    }
}

void __fastcall WorldCameraUpdateHook(void* self, void* method) {
    DataPointerProbe::StoreWorldCameraPointer(self, nullptr);
    if (g_original_world_camera_update != nullptr) {
        g_original_world_camera_update(self, method);
    }
}

void __fastcall WorldCameraInitHook(void* self, void* map, bool is_in_editor, void* method) {
    DataPointerProbe::StoreWorldCameraPointer(self, map);
    if (g_original_world_camera_init != nullptr) {
        g_original_world_camera_init(self, map, is_in_editor, method);
    }
}

}

bool DataPointerProbe::Install(std::uintptr_t game_assembly_base, Diagnostics& diagnostics) {
#ifndef ARCANUS_WITH_DX11_HOOK
    (void)game_assembly_base;
    diagnostics.Warn("DataPointerProbe disabled: MinHook backend not built");
    return false;
#else
    if (g_installed.load(std::memory_order_acquire)) {
        return true;
    }

    if (game_assembly_base == 0) {
        return false;
    }

    const auto target = game_assembly_base + offsets::rva::DataInit;
    const auto load_map_target = game_assembly_base + offsets::rva::DataLoadMap;
    const auto world_camera_update_target = game_assembly_base + offsets::rva::WorldCameraUpdate;
    const auto world_camera_init_target = game_assembly_base + offsets::rva::WorldCameraInit;
    if (!SafeMemory::IsProbablyUserPointer(target)) {
        diagnostics.Warn("DataPointerProbe target failed pointer validation");
        return false;
    }

    const auto init_status = MH_Initialize();
    if (init_status != MH_OK && init_status != MH_ERROR_ALREADY_INITIALIZED) {
        diagnostics.Warn("DataPointerProbe MinHook init failed");
        return false;
    }

    const auto create_status = MH_CreateHook(
        reinterpret_cast<void*>(target),
        reinterpret_cast<void*>(&DataInitHook),
        reinterpret_cast<void**>(&g_original_data_init));
    if (create_status != MH_OK && create_status != MH_ERROR_ALREADY_CREATED) {
        diagnostics.Warn("DataPointerProbe hook create failed at Data.bfny");
        return false;
    }

    const auto enable_status = MH_EnableHook(reinterpret_cast<void*>(target));
    if (enable_status != MH_OK && enable_status != MH_ERROR_ENABLED) {
        diagnostics.Warn("DataPointerProbe hook enable failed at Data.bfny");
        return false;
    }

    const auto create_map_status = MH_CreateHook(
        reinterpret_cast<void*>(load_map_target),
        reinterpret_cast<void*>(&DataLoadMapHook),
        reinterpret_cast<void**>(&g_original_data_load_map));
    if (create_map_status != MH_OK && create_map_status != MH_ERROR_ALREADY_CREATED) {
        diagnostics.Warn("DataPointerProbe hook create failed at Data.bfob");
    } else {
        const auto enable_map_status = MH_EnableHook(reinterpret_cast<void*>(load_map_target));
        if (enable_map_status != MH_OK && enable_map_status != MH_ERROR_ENABLED) {
            diagnostics.Warn("DataPointerProbe hook enable failed at Data.bfob");
        } else {
            g_data_load_map_hook_address = load_map_target;
        }
    }

    g_data_init_hook_address = target;

    const auto create_world_camera_update_status = MH_CreateHook(
        reinterpret_cast<void*>(world_camera_update_target),
        reinterpret_cast<void*>(&WorldCameraUpdateHook),
        reinterpret_cast<void**>(&g_original_world_camera_update));
    if (create_world_camera_update_status != MH_OK && create_world_camera_update_status != MH_ERROR_ALREADY_CREATED) {
        diagnostics.Warn("DataPointerProbe hook create failed at BhWorldCamera.Update");
    } else {
        const auto enable_world_camera_update_status = MH_EnableHook(reinterpret_cast<void*>(world_camera_update_target));
        if (enable_world_camera_update_status != MH_OK && enable_world_camera_update_status != MH_ERROR_ENABLED) {
            diagnostics.Warn("DataPointerProbe hook enable failed at BhWorldCamera.Update");
        } else {
            g_world_camera_update_hook_address = world_camera_update_target;
        }
    }

    const auto create_world_camera_init_status = MH_CreateHook(
        reinterpret_cast<void*>(world_camera_init_target),
        reinterpret_cast<void*>(&WorldCameraInitHook),
        reinterpret_cast<void**>(&g_original_world_camera_init));
    if (create_world_camera_init_status != MH_OK && create_world_camera_init_status != MH_ERROR_ALREADY_CREATED) {
        diagnostics.Warn("DataPointerProbe hook create failed at BhWorldCamera.Init");
    } else {
        const auto enable_world_camera_init_status = MH_EnableHook(reinterpret_cast<void*>(world_camera_init_target));
        if (enable_world_camera_init_status != MH_OK && enable_world_camera_init_status != MH_ERROR_ENABLED) {
            diagnostics.Warn("DataPointerProbe hook enable failed at BhWorldCamera.Init");
        } else {
            g_world_camera_init_hook_address = world_camera_init_target;
        }
    }

    g_installed.store(true, std::memory_order_release);
    diagnostics.Info("DataPointerProbe installed on Data + BhWorldCamera hooks");
    return true;
#endif
}

void DataPointerProbe::Shutdown(Diagnostics& diagnostics) {
#ifdef ARCANUS_WITH_DX11_HOOK
    if (g_installed.exchange(false, std::memory_order_acq_rel)) {
        if (g_data_init_hook_address != 0) {
            MH_DisableHook(reinterpret_cast<void*>(g_data_init_hook_address));
        }
        if (g_data_load_map_hook_address != 0) {
            MH_DisableHook(reinterpret_cast<void*>(g_data_load_map_hook_address));
        }
        if (g_world_camera_update_hook_address != 0) {
            MH_DisableHook(reinterpret_cast<void*>(g_world_camera_update_hook_address));
        }
        if (g_world_camera_init_hook_address != 0) {
            MH_DisableHook(reinterpret_cast<void*>(g_world_camera_init_hook_address));
        }
        diagnostics.Info("DataPointerProbe disabled");
    }
#else
    (void)diagnostics;
#endif
}

std::uintptr_t DataPointerProbe::DataPointer() {
    return g_data_ptr.load(std::memory_order_acquire);
}

std::uintptr_t DataPointerProbe::MapDataPointer() {
    return g_map_data_ptr.load(std::memory_order_acquire);
}

std::uintptr_t DataPointerProbe::WorldCameraPointer() {
    return g_world_camera_ptr.load(std::memory_order_acquire);
}

std::uintptr_t DataPointerProbe::UnityCameraPointer() {
    return g_unity_camera_ptr.load(std::memory_order_acquire);
}

std::uintptr_t DataPointerProbe::WorldMapPointer() {
    return g_world_map_ptr.load(std::memory_order_acquire);
}

std::string DataPointerProbe::Status() {
    if (!g_installed.load(std::memory_order_acquire)) {
        return "data hook not installed";
    }
    const auto ptr = DataPointer();
    if (ptr == 0) {
        return "data hook active | waiting Data.bfny";
    }
    std::string status = MapDataPointer() == 0 ? "data hook captured Data*" : "data hook captured Data* + MapData*";
    if (WorldCameraPointer() != 0 && UnityCameraPointer() != 0 && WorldMapPointer() != 0) {
        status += " + WorldCamera";
    }
    return status;
}

void DataPointerProbe::StoreDataPointer(void* self, void* init_context) {
    const auto ptr = reinterpret_cast<std::uintptr_t>(self);
    if (SafeMemory::IsProbablyUserPointer(ptr)) {
        g_data_ptr.store(ptr, std::memory_order_release);
    }

    const auto context = reinterpret_cast<std::uintptr_t>(init_context);
    if (SafeMemory::IsProbablyUserPointer(context)) {
        const auto map_data = SafeMemory::ReadPtr(context + offsets::data_init_context::MapData);
        if (map_data) {
            StoreMapDataPointer(reinterpret_cast<void*>(*map_data));
        }
    }
}

void DataPointerProbe::StoreMapDataPointer(void* map_data) {
    const auto ptr = reinterpret_cast<std::uintptr_t>(map_data);
    if (SafeMemory::IsProbablyUserPointer(ptr)) {
        g_map_data_ptr.store(ptr, std::memory_order_release);
    }
}

void DataPointerProbe::StoreWorldCameraPointer(void* world_camera, void* map) {
    const auto ptr = reinterpret_cast<std::uintptr_t>(world_camera);
    if (!SafeMemory::IsProbablyUserPointer(ptr)) {
        return;
    }

    g_world_camera_ptr.store(ptr, std::memory_order_release);

    if (const auto camera_ptr = SafeMemory::ReadPtr(ptr + offsets::world_camera::TargetCamera);
        camera_ptr && SafeMemory::IsProbablyUserPointer(*camera_ptr)) {
        g_unity_camera_ptr.store(*camera_ptr, std::memory_order_release);
    }

    const auto map_ptr = reinterpret_cast<std::uintptr_t>(map);
    if (SafeMemory::IsProbablyUserPointer(map_ptr)) {
        g_world_map_ptr.store(map_ptr, std::memory_order_release);
    } else if (const auto stored_map_ptr = SafeMemory::ReadPtr(ptr + offsets::world_camera::Map);
        stored_map_ptr && SafeMemory::IsProbablyUserPointer(*stored_map_ptr)) {
        g_world_map_ptr.store(*stored_map_ptr, std::memory_order_release);
    }
}

}
