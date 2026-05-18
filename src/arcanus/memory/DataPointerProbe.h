#pragma once

#include "arcanus/diagnostics/Diagnostics.h"

#include <atomic>
#include <cstdint>
#include <string>

namespace arcanus {

class DataPointerProbe {
public:
    static bool Install(std::uintptr_t game_assembly_base, Diagnostics& diagnostics);
    static void Shutdown(Diagnostics& diagnostics);
    static std::uintptr_t DataPointer();
    static std::uintptr_t MapDataPointer();
    static std::uintptr_t WorldCameraPointer();
    static std::uintptr_t UnityCameraPointer();
    static std::uintptr_t WorldMapPointer();
    static std::string Status();
    static void StoreDataPointer(void* self, void* init_context);
    static void StoreMapDataPointer(void* map_data);
    static void StoreWorldCameraPointer(void* world_camera, void* map);
};

}
