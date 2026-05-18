# ARCANUS display aliases

Purpose: keep ESP and reward-preview labels readable. The overlay uses
`src/arcanus/core/DisplayNames.cpp` as the runtime alias table, and this CSV is
the editable dataset mirror.

Sources used for the first pass:
- Official wiki spell category: https://wiki.hoodedhorse.com/Heroes_of_Might_and_Magic_Olden_Era/Category%3ASpells
- Olden Era spell list: https://www.olden-era.com/en/spells
- Olden Era map objects: https://heroes-olden-era.ru/en/mapObject
- Olden Era artifact list: https://www.olden-era.com/en/artefacts

Rules:
- Exact aliases win first.
- Generic SID cleanup removes prefixes such as `resource_`, `spell_`,
  `magic_`, `item_`, `unit_`, `mine_`, and school-tier prefixes such as
  `night_2_`.
- Unknown IDs are converted from snake_case to Title Case and truncated.
