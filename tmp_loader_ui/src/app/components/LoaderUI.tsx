import { useState, useEffect } from 'react';
import { Rocket, Syringe, Settings, FolderOpen, RefreshCw, Eye, Triangle } from 'lucide-react';
import * as Progress from '@radix-ui/react-progress';
import * as ScrollArea from '@radix-ui/react-scroll-area';

type LoadingAction = {
  label: string;
  progress: number;
} | null;

export function LoaderUI() {
  const [loadingAction, setLoadingAction] = useState<LoadingAction>(null);

  const handleAction = (label: string) => {
    setLoadingAction({ label, progress: 0 });

    // Simulate progress
    const interval = setInterval(() => {
      setLoadingAction((prev) => {
        if (!prev) return null;
        const newProgress = prev.progress + Math.random() * 15;
        if (newProgress >= 100) {
          clearInterval(interval);
          setTimeout(() => setLoadingAction(null), 500);
          return { ...prev, progress: 100 };
        }
        return { ...prev, progress: newProgress };
      });
    }, 200);
  };

  return (
    <div className="min-h-screen bg-[#0a0a0a] text-[#ff8c00] p-8 font-mono">
      <div className="max-w-7xl mx-auto">
        {/* Header */}
        <div className="mb-8 flex items-center gap-4">
          <div className="relative">
            <Triangle className="w-12 h-12 text-[#ff8c00]" />
            <Eye className="w-6 h-6 text-[#ff8c00] absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2" />
          </div>
          <div>
            <h1 className="text-4xl font-bold tracking-wider text-[#ff8c00]">ARCANUS LOADER</h1>
            <p className="text-sm text-[#888888] mt-1">
              Читающий тактический помощник для Heroes of Might and Magic: Olden Era
            </p>
          </div>
        </div>

        {/* Progress Bar Modal */}
        {loadingAction && (
          <div className="fixed inset-0 bg-black/80 flex items-center justify-center z-50">
            <div className="border-2 border-[#ff8c00] bg-[#0a0a0a] p-8 min-w-[400px]">
              <h3 className="text-lg text-[#ff8c00] mb-4 text-center">{loadingAction.label}</h3>
              <Progress.Root className="relative h-6 w-full bg-[#2a2a2a] border-2 border-[#ff8c00] overflow-hidden">
                <Progress.Indicator
                  className="h-full bg-[#ff8c00] transition-all duration-300 ease-out"
                  style={{ width: `${loadingAction.progress}%` }}
                />
              </Progress.Root>
              <div className="text-center text-[#ff8c00] mt-3 text-sm">
                {Math.round(loadingAction.progress)}%
              </div>
            </div>
          </div>
        )}

        {/* Main Content Grid */}
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
          {/* Left Panel - Status and Actions */}
          <div className="border-2 border-[#ff8c00] bg-[#1a1a1a] p-6 flex flex-col h-[600px]">
            <h2 className="text-xl font-bold mb-4 pb-2 border-b-2 border-[#ff8c00]">
              СТАТУС
            </h2>

            <ScrollArea.Root className="flex-1 overflow-hidden">
              <ScrollArea.Viewport className="w-full h-full">
                <div className="mb-6">
                  <div className="text-lg text-[#ff8c00] mb-4">
                    Игра запущена, оверлей внедрен.
                  </div>
                </div>

                {/* Action Buttons */}
                <div className="space-y-3 mb-8">
              <button
                onClick={() => handleAction('Запуск игры...')}
                className="w-full bg-[#2a2a2a] border-2 border-[#ff8c00] text-[#ff8c00] px-4 py-3 hover:bg-[#ff8c00] hover:text-[#0a0a0a] transition-all flex items-center gap-3 group"
              >
                <Rocket className="w-5 h-5" />
                <span>Запустить игру</span>
              </button>

              <button
                onClick={() => handleAction('Внедрение оверлея...')}
                className="w-full bg-[#2a2a2a] border-2 border-[#ff8c00] text-[#ff8c00] px-4 py-3 hover:bg-[#ff8c00] hover:text-[#0a0a0a] transition-all flex items-center gap-3 group"
              >
                <Syringe className="w-5 h-5" />
                <span>Внедрить оверлей</span>
              </button>

              <button
                onClick={() => handleAction('Запуск и внедрение...')}
                className="w-full bg-[#2a2a2a] border-2 border-[#ff8c00] text-[#ff8c00] px-4 py-3 hover:bg-[#ff8c00] hover:text-[#0a0a0a] transition-all flex items-center gap-3 group"
              >
                <Settings className="w-5 h-5" />
                <span>Запуск + Внедрение</span>
              </button>

              <button
                onClick={() => handleAction('Открытие папки...')}
                className="w-full bg-[#2a2a2a] border-2 border-[#ff8c00] text-[#ff8c00] px-4 py-3 hover:bg-[#ff8c00] hover:text-[#0a0a0a] transition-all flex items-center gap-3 group"
              >
                <FolderOpen className="w-5 h-5" />
                <span>Открыть папку игры</span>
              </button>

              <button
                onClick={() => handleAction('Обновление...')}
                className="w-full bg-[#2a2a2a] border-2 border-[#ff8c00] text-[#ff8c00] px-4 py-3 hover:bg-[#ff8c00] hover:text-[#0a0a0a] transition-all flex items-center gap-3 group"
              >
                <RefreshCw className="w-5 h-5" />
                <span>Обновить</span>
              </button>
            </div>

                {/* Footer */}
                <div className="pt-4 border-t-2 border-[#ff8c00]">
                  <p className="text-xs text-[#888888] leading-relaxed">
                    Горячие клавиши: F1 - HUD, ~ - Dev Panel, End - Выгрузить.
                    <br />
                    Не редактирует игровые значения.
                  </p>
                </div>
              </ScrollArea.Viewport>
              <ScrollArea.Scrollbar
                className="flex select-none touch-none p-0.5 bg-[#2a2a2a] transition-colors duration-150 ease-out hover:bg-[#3a3a3a] data-[orientation=vertical]:w-2.5 data-[orientation=horizontal]:flex-col data-[orientation=horizontal]:h-2.5"
                orientation="vertical"
              >
                <ScrollArea.Thumb className="flex-1 bg-[#ff8c00] rounded-[10px] relative before:content-[''] before:absolute before:top-1/2 before:left-1/2 before:-translate-x-1/2 before:-translate-y-1/2 before:w-full before:h-full before:min-w-[44px] before:min-h-[44px]" />
              </ScrollArea.Scrollbar>
            </ScrollArea.Root>
          </div>

          {/* Right Panel - Patch Notes */}
          <div className="border-2 border-[#ff8c00] bg-[#1a1a1a] p-6 flex flex-col h-[600px]">
            <h2 className="text-xl font-bold mb-4 pb-2 border-b-2 border-[#ff8c00]">
              ПРИМЕЧАНИЯ К ПАТЧУ
            </h2>

            <ScrollArea.Root className="flex-1 overflow-hidden">
              <ScrollArea.Viewport className="w-full h-full">
                <div className="space-y-6 pr-4">
              <div>
                <h3 className="text-lg font-bold text-[#ff8c00] mb-3">
                  # Текущая сборка
                </h3>
                <ul className="space-y-2">
                  {[
                    'Новый GUI загрузчика (image_0.png)',
                    'Читающий режим: без записи значений',
                    'Улучшенная привязка ESP к объектам Unity',
                    'Отключены debug-стрелки маршрута',
                    'Вкладки в Dev Panel (Статус, ESP, и т.д.)',
                    'Удаленный Ollama-мост активен'
                  ].map((item, i) => (
                    <li key={i} className="flex items-start gap-2 text-sm text-[#cccccc]">
                      <span className="text-[#ff8c00] mt-1">▸</span>
                      <span>{item}</span>
                    </li>
                  ))}
                </ul>
              </div>

              <div className="border-t-2 border-[#ff8c00] pt-4">
                <h3 className="text-lg font-bold text-[#ff8c00] mb-3">
                  # Далее
                </h3>
                <ul className="space-y-2">
                  {[
                    'Продвинутый путь с поиском препятствий',
                    'Расширенный анализ карты',
                    'Автоматическое обнаружение ресурсов'
                  ].map((item, i) => (
                    <li key={i} className="flex items-start gap-2 text-sm text-[#cccccc]">
                      <span className="text-[#ff8c00] mt-1">▸</span>
                      <span>{item}</span>
                    </li>
                  ))}
                </ul>
              </div>
                </div>
              </ScrollArea.Viewport>
              <ScrollArea.Scrollbar
                className="flex select-none touch-none p-0.5 bg-[#2a2a2a] transition-colors duration-150 ease-out hover:bg-[#3a3a3a] data-[orientation=vertical]:w-2.5 data-[orientation=horizontal]:flex-col data-[orientation=horizontal]:h-2.5"
                orientation="vertical"
              >
                <ScrollArea.Thumb className="flex-1 bg-[#ff8c00] rounded-[10px] relative before:content-[''] before:absolute before:top-1/2 before:left-1/2 before:-translate-x-1/2 before:-translate-y-1/2 before:w-full before:h-full before:min-w-[44px] before:min-h-[44px]" />
              </ScrollArea.Scrollbar>
            </ScrollArea.Root>
          </div>
        </div>
      </div>
    </div>
  );
}
