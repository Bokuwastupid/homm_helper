import { useState } from 'react';
import { LoaderUI } from './components/LoaderUI';
import { OverlayUI } from './components/OverlayUI';

export default function App() {
  const [view, setView] = useState<'loader' | 'overlay'>('loader');

  return (
    <div className="size-full">
      {/* View Toggle */}
      <div className="fixed top-4 right-4 z-50 flex gap-2">
        <button
          onClick={() => setView('loader')}
          className={`px-4 py-2 border-2 font-mono transition-all ${
            view === 'loader'
              ? 'bg-[#ff8c00] text-[#0a0a0a] border-[#ff8c00]'
              : 'bg-[#1a1a1a] text-[#ff8c00] border-[#ff8c00] hover:bg-[#2a2a2a]'
          }`}
        >
          Загрузчик
        </button>
        <button
          onClick={() => setView('overlay')}
          className={`px-4 py-2 border-2 font-mono transition-all ${
            view === 'overlay'
              ? 'bg-[#ff8c00] text-[#0a0a0a] border-[#ff8c00]'
              : 'bg-[#1a1a1a] text-[#ff8c00] border-[#ff8c00] hover:bg-[#2a2a2a]'
          }`}
        >
          Оверлей
        </button>
      </div>

      {view === 'loader' ? <LoaderUI /> : <OverlayUI />}
    </div>
  );
}