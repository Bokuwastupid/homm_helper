# ARCANUS Overlay

Read-only tactical companion overlay for **Heroes of Might and Magic: Olden Era**.

Current scope:

- DLL container for an in-game overlay.
- GUI `arcanus_loader.exe` with game launch, overlay injection, process status, and patch notes.
- DX11 `Present` / `ResizeBuffers` hook through MinHook.
- Dear ImGui HUD and developer panel.
- Lock-free snapshot handoff for render-safe `GameState` / advice reads.
- Read-only memory scanner skeleton with safe memory helpers.
- JSON game-state model.
- Tactical advisor heuristics.
- Background `Core.zip` database indexing.
- In-process Ollama is disabled by default; the next safe AI path is an external bridge process.
- UTF-16 to UTF-8 helpers for IL2CPP strings.

Out of scope by design:

- Writing game memory.
- Changing resources, movement, units, battle results, or other gameplay values.
- Stealth injection, anti-cheat bypasses, or PvP automation.

## Build Layout

The project builds:

- `arcanus_loader.exe`
- `arcanus_overlay.dll`

Configure and build:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Outputs:

```text
build/Release/arcanus_loader.exe
build/Release/arcanus_overlay.dll
```

## Run

1. Run `build/Release/arcanus_loader.exe`.
2. Use `Start Game`, `Inject Overlay`, or `Start + Inject`.
3. Use hotkeys in game.

## Runtime Keys

- `F1` toggles the ARCANUS overlay flag.
- `` ` `` toggles the developer panel flag.
- `F3` marks a battle-sim request in the current state pipeline.
- `End` hides HUD / Dev Panel safely. Runtime DLL unload is disabled because unloading live DX11 hooks can crash the game.

## Data Notes

MVP 1 runs on mock state plus static database indexing. Real map/combat memory reads are staged for MVP 3/4.

Current map support reads live hero/resources/army, map object nodes, and reward-set previews when `DataRewardSet.objectId` matches a visible map object id. ESP projection uses the game's node-to-world and camera projection functions every frame within a budget, so zoom and camera pan should stay aligned without stale screen-position cache.

Useful reverse-engineering inputs:

- `GameAssembly.dll`
- `HeroesOldenEra_Data/il2cpp_data/Metadata/global-metadata.dat`
- Il2CppDumper output: `dump.cs`, `script.json`, `il2cpp.h`, `DummyDll`
- `GAME_STRUCTURES.md`
- `MEMORY_MAP.md`
- Optional Ghidra MCP session with `GameAssembly.dll` opened

Known offsets from those notes are captured in `src/arcanus/memory/KnownOffsets.h`.

## Safety Model

- Render thread never waits on scanner, database, or Ollama.
- `ResizeBuffers` releases render-target resources before calling the original function.
- ImGui mouse/keyboard capture blocks click-through into the game.
- Memory reads go through safe wrappers.
- Game data remains untouched.
