# Reverse Engineering Inputs

ARCANUS MVP 3/4 needs stable read-only pointers and signatures. The current implementation already contains the offset constants from the supplied notes, but does not yet enable live gameplay reads because `Data*` discovery still needs validation in Ghidra/runtime.

## Supplied Notes

- `C:/Users/bokuw/Downloads/GAME_STRUCTURES.md`
- `C:/Users/bokuw/Downloads/MEMORY_MAP.md`

The useful technical pieces from those files are now represented in:

- `src/arcanus/memory/KnownOffsets.h`
- `src/arcanus/memory/SafeMemory.h`
- `src/arcanus/memory/ReadOnlyScanner.cpp`

## Needed For MVP 3

- Current Steam `GameAssembly.dll`
- Current `global-metadata.dat`
- Il2CppDumper output:
  - `dump.cs`
  - `script.json`
  - `DummyDll`
  - optional generated headers/scripts
- Ghidra project with `GameAssembly.dll` auto-analyzed.

## First Live Pointer Strategy

Preferred route:

1. Find or hook a function that receives `Data*` as `this`, such as `Data.bfny` or `Data.Save`.
2. Store only the observed pointer in `LivePointers`.
3. Let `ReadOnlyScanner` read through validated pointer chains.
4. If validation fails, keep the last mock/valid snapshot and show `[Pointer Error]`.

This project does not write gameplay memory and does not call gameplay mutator functions.

