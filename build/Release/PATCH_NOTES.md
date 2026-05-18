# ARCANUS patch notes

## Current build
- Новый стиль loader: тёмная панель, оранжевый HUD-акцент, Start Game / Inject Overlay / Start + Inject.
- `End` больше не выгружает DLL из живой игры. Теперь клавиша безопасно прячет HUD и Dev Panel.
- Overlay остаётся read-only: без записи ресурсов, перемещения, HP, армии или результата боя.
- ESP больше не использует устаревающий screen-position cache. Объекты проектируются заново через Unity camera каждый кадр в пределах бюджета.
- Pandora, scroll box, обычные сундуки и encounter-группы разделены фильтрами в Dev Panel.
- Reward preview получил диагностику: total / linked / unmatched и raw preview в ESP/Scanner вкладках.
- In-DLL Ollama отключена: следующий безопасный вариант — внешний `arcanus_ai_bridge.exe`.

## Next
- Настоящий pathfinding по walkability / roads / obstacles вместо прямых debug-линий.
- Более точный reward inspector для Pandora box, scroll box и encounter-объектов.
- Внешний AI bridge для локальной Ollama без сетевого/LLM-кода внутри DLL.
