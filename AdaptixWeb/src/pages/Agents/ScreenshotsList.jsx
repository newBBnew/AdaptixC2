import React, { useState, useEffect, useRef } from 'react';
import { 
  Image as ImageIcon, 
  Search, 
  Filter, 
  Trash2, 
  Download, 
  Edit3, 
  RefreshCw,
  X,
  Maximize2,
  ChevronLeft,
  ChevronRight,
  AlertCircle
} from 'lucide-react';
import { dataApi } from '../../api/control';
import { cn } from '../../utils/cn';
import { motion, AnimatePresence } from 'framer-motion';

const ScreenshotsList = () => {
  const [screenshots, setScreenshots] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [selectedId, setSelectedId] = useState(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const [splitterPos, setSplitterPos] = useState(400); // Initial table width
  const isResizing = useRef(false);

  const fetchScreenshots = async () => {
    try {
      setLoading(true);
      const response = await dataApi.screenshots();
      setScreenshots(Array.isArray(response.data) ? response.data : []);
      setError(null);
    } catch (err) {
      console.error('Failed to fetch screenshots:', err);
      setError('Connection failed');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchScreenshots();
    const interval = setInterval(fetchScreenshots, 30000);
    return () => clearInterval(interval);
  }, []);

  useEffect(() => {
    const handleMouseMove = (e) => {
      if (!isResizing.current) return;
      const newWidth = e.clientX - 64; // Adjust for sidebar
      if (newWidth > 200 && newWidth < window.innerWidth - 300) {
        setSplitterPos(newWidth);
      }
    };
    const handleMouseUp = () => {
      isResizing.current = false;
      document.body.style.cursor = 'default';
    };
    window.addEventListener('mousemove', handleMouseMove);
    window.addEventListener('mouseup', handleMouseUp);
    return () => {
      window.removeEventListener('mousemove', handleMouseMove);
      window.removeEventListener('mouseup', handleMouseUp);
    };
  }, []);

  const filteredScreenshots = screenshots.filter(s => 
    Object.values(s).some(val => 
      String(val).toLowerCase().includes(searchQuery.toLowerCase())
    )
  );

  const selectedScreen = screenshots.find(s => s.screen_id === selectedId);

  return (
    <div className="flex flex-col h-full bg-dark-900 text-gray-300 font-sans select-none overflow-hidden">
      {/* 1. Header with Controls (Mimics ScreenshotsWidget.cpp) */}
      <div className="flex items-center justify-between px-3 py-1.5 bg-dark-800 border-b border-dark-700 shrink-0">
        <div className="flex items-center space-x-3">
          <div className="flex items-center space-x-2 px-2 py-0.5 rounded bg-accent-primary/10 border border-accent-primary/20">
            <ImageIcon className="w-3.5 h-3.5 text-accent-primary" />
            <span className="text-[10px] font-black uppercase tracking-widest text-accent-primary">Screenshots</span>
          </div>
          <div className="h-4 w-px bg-dark-600" />
          <button 
            onClick={() => setIsSearchVisible(!isSearchVisible)}
            className={cn(
              "p-1 rounded hover:bg-dark-700 transition-colors",
              isSearchVisible ? "bg-accent-primary/20 text-accent-primary" : "text-gray-500"
            )}
            title="Toggle Search (Ctrl+F)"
          >
            <Search className="w-3.5 h-3.5" />
          </button>
        </div>

        <div className="flex items-center space-x-1">
          <button 
            onClick={fetchScreenshots}
            className="p-1.5 rounded hover:bg-dark-700 text-gray-400 hover:text-white transition-all"
            title="Refresh"
          >
            <RefreshCw className={cn("w-3.5 h-3.5", loading && "animate-spin text-accent-primary")} />
          </button>
        </div>
      </div>

      {/* 2. Main Splitter Layout */}
      <div className="flex-1 flex overflow-hidden">
        {/* Left: Table Area */}
        <div 
          style={{ width: `${splitterPos}px` }}
          className="flex flex-col border-r border-dark-700 overflow-hidden shrink-0"
        >
          {isSearchVisible && (
            <div className="px-3 py-2 bg-dark-800/50 border-b border-dark-700">
              <input 
                type="text" 
                autoFocus
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                placeholder="filter: host | user..." 
                className="w-full bg-dark-950/50 border border-dark-600 rounded px-3 py-1 text-[11px] text-gray-300 outline-none focus:border-accent-primary/50"
              />
            </div>
          )}
          <div className="flex-1 overflow-auto scrollbar-thin">
            <table className="w-full text-left border-collapse table-auto">
              <thead className="sticky top-0 bg-dark-800 z-10">
                <tr className="border-b border-dark-700 text-gray-500 text-[10px] font-bold uppercase tracking-tight">
                  <th className="py-2 px-4 border-r border-dark-700/30">Computer</th>
                  <th className="py-2 px-4 border-r border-dark-700/30">User</th>
                  <th className="py-2 px-4 border-r border-dark-700/30">Date</th>
                  <th className="py-2 px-4">Note</th>
                </tr>
              </thead>
              <tbody className="text-[11px] font-medium divide-y divide-dark-800/30">
                {filteredScreenshots.length === 0 ? (
                  <tr><td colSpan="4" className="py-12 text-center text-gray-600 italic">No screenshots</td></tr>
                ) : (
                  filteredScreenshots.map((s) => (
                    <tr 
                      key={s.screen_id} 
                      onClick={() => setSelectedId(s.screen_id)}
                      className={cn(
                        "hover:bg-accent-primary/5 transition-colors group h-8 cursor-pointer",
                        selectedId === s.screen_id && "bg-accent-primary/10 border-l-2 border-l-accent-primary"
                      )}
                    >
                      <td className="px-4 text-gray-300 truncate">{s.computer}</td>
                      <td className="px-4 text-gray-400 truncate">{s.username}</td>
                      <td className="px-4 text-gray-500 font-mono text-[10px]">
                        {new Date(s.date * 1000).toLocaleTimeString()}
                      </td>
                      <td className="px-4 text-gray-500 italic truncate">{s.note || '---'}</td>
                    </tr>
                  ))
                )}
              </tbody>
            </table>
          </div>
        </div>

        {/* Resizer Handle */}
        <div 
          className="w-1 bg-dark-700 hover:bg-accent-primary cursor-col-resize transition-colors z-20"
          onMouseDown={() => {
            isResizing.current = true;
            document.body.style.cursor = 'col-resize';
          }}
        />

        {/* Right: Image Preview (Mimics ImageFrame) */}
        <div className="flex-1 bg-[#050505] flex flex-col relative overflow-hidden">
          <div className="absolute top-2 right-2 z-10 flex space-x-1 opacity-0 group-hover:opacity-100 transition-opacity">
            {/* Action buttons would go here in a hover overlay */}
          </div>

          <div className="flex-1 flex items-center justify-center p-4 overflow-auto custom-scrollbar">
            <AnimatePresence mode="wait">
              {selectedScreen ? (
                <motion.div
                  key={selectedId}
                  initial={{ opacity: 0, scale: 0.95 }}
                  animate={{ opacity: 1, scale: 1 }}
                  exit={{ opacity: 0, scale: 1.05 }}
                  className="relative group"
                >
                  <img 
                    src={selectedScreen.content ? `data:image/png;base64,${selectedScreen.content}` : '/placeholder-screen.png'} 
                    alt="Screenshot Preview"
                    className="max-w-full max-h-full shadow-2xl rounded border border-dark-600"
                  />
                  <div className="absolute inset-0 bg-black/40 opacity-0 group-hover:opacity-100 transition-opacity flex items-center justify-center space-x-4">
                    <button className="p-2 bg-dark-800 rounded-full hover:bg-accent-primary text-white transition-all shadow-lg" title="Download">
                      <Download size={20} />
                    </button>
                    <button className="p-2 bg-dark-800 rounded-full hover:bg-accent-secondary text-white transition-all shadow-lg" title="Full Screen">
                      <Maximize2 size={20} />
                    </button>
                    <button className="p-2 bg-dark-800 rounded-full hover:bg-accent-danger text-white transition-all shadow-lg" title="Delete">
                      <Trash2 size={20} />
                    </button>
                  </div>
                </motion.div>
              ) : (
                <div className="flex flex-col items-center justify-center opacity-10">
                  <ImageIcon size={80} />
                  <p className="mt-4 text-sm font-black uppercase tracking-widest">Select a capture to view</p>
                </div>
              )}
            </AnimatePresence>
          </div>

          {selectedScreen && (
            <div className="px-4 py-2 bg-dark-800/80 border-t border-dark-700 flex items-center justify-between text-[10px] font-bold text-gray-400">
              <div className="flex items-center space-x-4">
                <span className="text-accent-primary uppercase tracking-tighter">Capture Detail</span>
                <span>{selectedScreen.computer} \ {selectedScreen.username}</span>
                <span className="text-gray-600 font-mono">{selectedScreen.screen_id}</span>
              </div>
              <div className="flex items-center space-x-2">
                <Edit3 size={12} className="text-gray-500 hover:text-white cursor-pointer" />
                <span className="italic">{selectedScreen.note || 'No note attached'}</span>
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

export default ScreenshotsList;
