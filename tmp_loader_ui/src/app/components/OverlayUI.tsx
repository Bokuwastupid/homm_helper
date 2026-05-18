import { useState } from 'react';
import {
  Activity,
  Scan,
  Database,
  Brain,
  FileText,
  ScrollText,
  Box,
  AlertTriangle,
  ChevronDown
} from 'lucide-react';
import * as Checkbox from '@radix-ui/react-checkbox';
import * as Slider from '@radix-ui/react-slider';
import * as DropdownMenu from '@radix-ui/react-dropdown-menu';
import * as ScrollArea from '@radix-ui/react-scroll-area';
import { Check } from 'lucide-react';

const tabs = [
  { id: 'status', label: 'Статус', icon: Activity },
  { id: 'esp', label: 'ESP', icon: Scan },
  { id: 'scanner', label: 'Сканнер', icon: Box },
  { id: 'database', label: 'База данных', icon: Database },
  { id: 'ai', label: 'ИИ', icon: Brain },
  { id: 'context', label: 'Контекст', icon: FileText },
  { id: 'logs', label: 'Логи', icon: ScrollText }
];

export function OverlayUI() {
  const [activeTab, setActiveTab] = useState('esp');
  const [uiScale, setUiScale] = useState([1.0]);

  const [espElements, setEspElements] = useState({
    worldObjects: true,
    routeLabels: false,
    heroMarker: true,
    calibrationGrid: false
  });

  const [layerFilters, setLayerFilters] = useState({
    resources: true,
    chests: true,
    mines: false,
    recruitment: false,
    cities: true,
    taverns: false,
    portals: false,
    prisons: false
  });

  return (
    <div className="min-h-screen bg-[url('https://images.unsplash.com/photo-1518709766631-a6a7f45921c3?w=1600&h=900&fit=crop')] bg-cover bg-center relative">
      {/* Game background overlay */}
      <div className="absolute inset-0 bg-black/40"></div>

      {/* ESP Markers on game world */}
      <div className="absolute top-1/4 left-1/3 w-16 h-16 border-2 border-[#ff8c00] bg-[#ff8c00]/10">
        <div className="absolute -top-6 left-0 text-xs text-[#ff8c00] bg-black/80 px-2 py-1 whitespace-nowrap">
          Ресурсы: Золото
        </div>
      </div>
      <div className="absolute top-1/2 left-1/2 w-12 h-12 border-2 border-[#ff8c00] bg-[#ff8c00]/10">
        <div className="absolute -top-6 left-0 text-xs text-[#ff8c00] bg-black/80 px-2 py-1 whitespace-nowrap">
          Город
        </div>
      </div>

      {/* Overlay Panel */}
      <div className="relative z-10 p-4">
        <div className="max-w-5xl mx-auto border-2 border-[#ff8c00] bg-[#0a0a0a]/95 shadow-2xl shadow-[#ff8c00]/20">
          {/* Header */}
          <div className="border-b-2 border-[#ff8c00] px-6 py-3 bg-[#1a1a1a]">
            <h1 className="text-xl font-bold text-[#ff8c00] tracking-wider font-mono">
              ARCANUS | Читающий тактический помощник
            </h1>
          </div>

          {/* Content Area */}
          <div className="flex">
            {/* Left Sidebar - Tabs */}
            <div className="w-48 border-r-2 border-[#ff8c00] bg-[#0a0a0a]">
              <div className="p-2 space-y-1">
                {tabs.map((tab) => {
                  const Icon = tab.icon;
                  return (
                    <button
                      key={tab.id}
                      onClick={() => setActiveTab(tab.id)}
                      className={`w-full px-3 py-2 flex items-center gap-2 transition-all text-left ${
                        activeTab === tab.id
                          ? 'bg-[#ff8c00] text-[#0a0a0a]'
                          : 'bg-[#2a2a2a] text-[#ff8c00] hover:bg-[#3a3a3a]'
                      }`}
                    >
                      <Icon className="w-4 h-4" />
                      <span className="text-sm">{tab.label}</span>
                    </button>
                  );
                })}
              </div>

              {/* Quick Status */}
              <div className="mt-6 border-t-2 border-[#ff8c00] p-3 space-y-2">
                <h3 className="text-xs font-bold text-[#ff8c00] mb-2">БЫСТРЫЙ СТАТУС</h3>
                <div className="space-y-1 text-xs">
                  <div className="flex items-start gap-2">
                    <AlertTriangle className="w-3 h-3 text-[#ff8c00] mt-0.5 flex-shrink-0" />
                    <span className="text-[#888888]">Состояние: ожидание данных</span>
                  </div>
                  <div className="flex items-start gap-2">
                    <AlertTriangle className="w-3 h-3 text-[#ff8c00] mt-0.5 flex-shrink-0" />
                    <span className="text-[#888888]">Сканнер: ошибка привязки</span>
                  </div>
                  <div className="flex items-start gap-2">
                    <AlertTriangle className="w-3 h-3 text-[#ff8c00] mt-0.5 flex-shrink-0" />
                    <span className="text-[#888888]">ИИ: простой</span>
                  </div>
                  <div className="flex items-start gap-2">
                    <AlertTriangle className="w-3 h-3 text-[#ff8c00] mt-0.5 flex-shrink-0" />
                    <span className="text-[#888888]">Живые данные не подключены</span>
                  </div>
                </div>
                <button className="w-full mt-3 px-3 py-1.5 bg-[#2a2a2a] border border-[#ff8c00] text-[#ff8c00] text-xs hover:bg-[#ff8c00] hover:text-[#0a0a0a] transition-all">
                  Объяснить статус
                </button>
              </div>
            </div>

            {/* Main Content */}
            <div className="flex-1 font-mono overflow-hidden">
              <ScrollArea.Root className="h-full">
                <ScrollArea.Viewport className="w-full h-full">
                  <div className="p-6">
                    {activeTab === 'esp' && (
                      <div className="space-y-6">
                  <h2 className="text-lg font-bold text-[#ff8c00] border-b-2 border-[#ff8c00] pb-2">
                    ESP ЭЛЕМЕНТЫ
                  </h2>

                  {/* Elements Section */}
                  <div>
                    <h3 className="text-sm font-bold text-[#ff8c00] mb-3">Элементы</h3>
                    <div className="grid grid-cols-2 gap-3">
                      {[
                        { key: 'worldObjects', label: 'Боксы объектов мира' },
                        { key: 'routeLabels', label: 'Лейблы маршрутов' },
                        { key: 'heroMarker', label: 'Маркер героя' },
                        { key: 'calibrationGrid', label: 'Сетка калибровки' }
                      ].map(({ key, label }) => (
                        <label key={key} className="flex items-center gap-2 text-sm cursor-pointer group">
                          <Checkbox.Root
                            checked={espElements[key as keyof typeof espElements]}
                            onCheckedChange={(checked) =>
                              setEspElements(prev => ({ ...prev, [key]: checked === true }))
                            }
                            className="w-5 h-5 border-2 border-[#ff8c00] bg-[#2a2a2a] flex items-center justify-center data-[state=checked]:bg-[#ff8c00]"
                          >
                            <Checkbox.Indicator>
                              <Check className="w-4 h-4 text-[#0a0a0a]" />
                            </Checkbox.Indicator>
                          </Checkbox.Root>
                          <span className="text-[#cccccc] group-hover:text-[#ff8c00]">{label}</span>
                        </label>
                      ))}
                    </div>
                  </div>

                  {/* Layer Filters */}
                  <div className="border-t-2 border-[#ff8c00] pt-4">
                    <h3 className="text-sm font-bold text-[#ff8c00] mb-3">Фильтры слоев</h3>
                    <div className="grid grid-cols-2 gap-3">
                      {[
                        { key: 'resources', label: 'Ресурсы' },
                        { key: 'chests', label: 'Сундуки' },
                        { key: 'mines', label: 'Шахты' },
                        { key: 'recruitment', label: 'Наем' },
                        { key: 'cities', label: 'Города' },
                        { key: 'taverns', label: 'Таверны' },
                        { key: 'portals', label: 'Порталы' },
                        { key: 'prisons', label: 'Тюрьмы' }
                      ].map(({ key, label }) => (
                        <label key={key} className="flex items-center gap-2 text-sm cursor-pointer group">
                          <Checkbox.Root
                            checked={layerFilters[key as keyof typeof layerFilters]}
                            onCheckedChange={(checked) =>
                              setLayerFilters(prev => ({ ...prev, [key]: checked === true }))
                            }
                            className="w-5 h-5 border-2 border-[#ff8c00] bg-[#2a2a2a] flex items-center justify-center data-[state=checked]:bg-[#ff8c00]"
                          >
                            <Checkbox.Indicator>
                              <Check className="w-4 h-4 text-[#0a0a0a]" />
                            </Checkbox.Indicator>
                          </Checkbox.Root>
                          <span className="text-[#cccccc] group-hover:text-[#ff8c00]">{label}</span>
                        </label>
                      ))}
                    </div>
                  </div>

                  {/* UI Scale */}
                  <div className="border-t-2 border-[#ff8c00] pt-4">
                    <div className="flex items-center justify-between mb-2">
                      <h3 className="text-sm font-bold text-[#ff8c00]">Шкала UI</h3>
                      <span className="text-sm text-[#ff8c00]">{uiScale[0].toFixed(2)}</span>
                    </div>
                    <Slider.Root
                      value={uiScale}
                      onValueChange={setUiScale}
                      min={0.5}
                      max={2.0}
                      step={0.01}
                      className="relative flex items-center w-full h-6"
                    >
                      <Slider.Track className="relative h-2 w-full bg-[#2a2a2a] border-2 border-[#ff8c00]">
                        <Slider.Range className="absolute h-full bg-[#ff8c00]" />
                      </Slider.Track>
                      <Slider.Thumb className="block w-4 h-4 bg-[#ff8c00] border-2 border-[#ff8c00] hover:scale-110 transition-transform" />
                    </Slider.Root>
                  </div>

                  {/* Calibration Dropdown */}
                  <div className="border-t-2 border-[#ff8c00] pt-4">
                    <DropdownMenu.Root>
                      <DropdownMenu.Trigger className="w-full px-4 py-2 bg-[#2a2a2a] border-2 border-[#ff8c00] text-[#ff8c00] hover:bg-[#ff8c00] hover:text-[#0a0a0a] transition-all flex items-center justify-center gap-2">
                        <span>Ручная калибровка (fallback)</span>
                        <ChevronDown className="w-4 h-4" />
                      </DropdownMenu.Trigger>

                      <DropdownMenu.Portal>
                        <DropdownMenu.Content
                          className="min-w-[300px] bg-[#0a0a0a] border-2 border-[#ff8c00] p-2 shadow-lg shadow-[#ff8c00]/20 z-50"
                          sideOffset={5}
                        >
                          <DropdownMenu.Item className="px-3 py-2 text-sm text-[#ff8c00] hover:bg-[#2a2a2a] outline-none cursor-pointer">
                            Калибровка по углам экрана
                          </DropdownMenu.Item>
                          <DropdownMenu.Item className="px-3 py-2 text-sm text-[#ff8c00] hover:bg-[#2a2a2a] outline-none cursor-pointer">
                            Калибровка по центру
                          </DropdownMenu.Item>
                          <DropdownMenu.Item className="px-3 py-2 text-sm text-[#ff8c00] hover:bg-[#2a2a2a] outline-none cursor-pointer">
                            Автоматическое выравнивание
                          </DropdownMenu.Item>
                          <DropdownMenu.Separator className="h-[2px] bg-[#ff8c00] my-1" />
                          <DropdownMenu.Item className="px-3 py-2 text-sm text-[#ff8c00] hover:bg-[#2a2a2a] outline-none cursor-pointer">
                            Сброс калибровки
                          </DropdownMenu.Item>
                          <DropdownMenu.Item className="px-3 py-2 text-sm text-[#ff8c00] hover:bg-[#2a2a2a] outline-none cursor-pointer">
                            Импорт настроек
                          </DropdownMenu.Item>
                          <DropdownMenu.Item className="px-3 py-2 text-sm text-[#ff8c00] hover:bg-[#2a2a2a] outline-none cursor-pointer">
                            Экспорт настроек
                          </DropdownMenu.Item>
                        </DropdownMenu.Content>
                      </DropdownMenu.Portal>
                    </DropdownMenu.Root>
                  </div>
                </div>
              )}

                    {activeTab !== 'esp' && (
                      <div className="text-center text-[#888888] py-12">
                        <p>Содержимое вкладки "{tabs.find(t => t.id === activeTab)?.label}" находится в разработке</p>
                      </div>
                    )}
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
    </div>
  );
}
