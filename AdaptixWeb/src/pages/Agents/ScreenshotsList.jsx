import React, { useState, useEffect, useRef } from 'react';
import { 
  Image as ImageIcon, 
  Search, 
  Trash2, 
  Download, 
  Edit3, 
  RefreshCw,
  X,
  Maximize2,
  FileImage
} from 'lucide-react';
import { dataApi } from '../../api/control';
import { cn } from '../../utils/cn';
import { motion, AnimatePresence } from 'framer-motion';
import ContextMenu from '../../components/ContextMenu';
import { useAgents } from '../../context/AgentContext';

const ScreenshotsList = () => {
  const { screenshots, fetchAgents, globalSearchQuery } = useAgents();
  const [loading, setLoading] = useState(false);
  const [selectedId, setSelectedId] = useState(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const [splitterPos, setSplitterPos] = useState(400); 
  const [menu, setMenu] = useState(null);
  const [isPreviewOpen, setIsPreviewOpen] = useState(false);
  const isResizing = useRef(false);

  useEffect(() => {
    const handleMouseMove = (e) => {
      if (!isResizing.current) return;
      const newWidth = e.clientX - 64; 
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

  const handleSetNote = async (screen) => {
    const newNote = window.prompt('Enter new note:', screen.s_note || '');
    if (newNote !== null) {
      try {
        await dataApi.setScreenshotNote([screen.s_screen_id], newNote);
      } catch (err) {
        console.error('Failed to set note:', err);
      }
    }
  };

  const handleDelete = async (id) => {
    if (!window.confirm('Are you sure you want to delete this screenshot?')) return;
    try {
      await dataApi.removeScreenshot([id]);
      if (selectedId === id) setSelectedId(null);
    } catch (err) {
      console.error('Failed to delete screenshot:', err);
    }
  };

  const handleDownload = (screen) => {
    if (!screen.s_content) return;
    const binaryString = atob(screen.s_content);
    const bytes = new Uint8Array(binaryString.length);
    for (let i = 0; i < binaryString.length; i++) {
      bytes[i] = binaryString.charCodeAt(i);
    }
    const blob = new Blob([bytes], { type: 'image/png' });
    const url = window.URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.setAttribute('download', `screenshot_${screen.s_computer}_${screen.s_screen_id.substring(0,6)}.png`);
    document.body.appendChild(link);
    link.click();
    link.remove();
    window.URL.revokeObjectURL(url);
  };

  const handleContextMenu = (e, screen) => {
    e.preventDefault();
    setSelectedId(screen.s_screen_id);
    setMenu({
      x: e.clientX,
      y: e.clientY,
      options: [
        { label: 'Set note', icon: Edit3, onClick: () => handleSetNote(screen) },
        { label: 'Download', icon: Download, onClick: () => handleDownload(screen) },
        { divider: true },
        { label: 'Delete', icon: Trash2, color: 'text-theme-danger', onClick: () => handleDelete(screen.s_screen_id) },
      ]
    });
  };

  const filteredScreenshots = screenshots.filter(s => {
    const query = (searchQuery || globalSearchQuery).toLowerCase();
    return Object.values(s).some(val => 
      String(val).toLowerCase().includes(query)
    );
  });

  const selectedScreen = screenshots.find(s => s.s_screen_id === selectedId);

  return (
    <div className="flex flex-col h-full w-full select-none overflow-hidden" onClick={() => setMenu(null)}>
      <header className="flex items-center justify-between px-3 py-2 glass-card-sm border-b border-theme-glass-light shrink-0">
        <div className="flex items-center space-x-3">
          <button 
            onClick={() => setIsSearchVisible(!isSearchVisible)}
            className={cn(
              "p-2 rounded-xl transition-all",
              isSearchVisible ? "bg-theme-accent/20 text-theme-accent border border-theme-accent/30" : "text-theme-muted hover:text-theme-primary hover:bg-theme-hover"
            )}
            title="Toggle Search (Ctrl+F)"
          >
            <Search className="w-4 h-4 text-theme-accent" />
          </button>
          <div className="h-5 w-px bg-theme-glass-light mx-1" />
          <button 
            onClick={() => {
              setLoading(true);
              fetchAgents().finally(() => setLoading(false));
            }}
            className="p-2 glass-btn text-theme-muted hover:text-theme-accent transition-all"
            title="Refresh Screenshots"
          >
            <RefreshCw className={cn("w-4 h-4", loading && "animate-spin text-theme-accent")} />
          </button>
        </div>
        <div className="flex items-center space-x-2">
          <span className="glass-btn px-3 py-1 text-xs font-black uppercase tracking-widest text-theme-accent-secondary border-theme-accent-secondary/30">Auto Capture Off</span>
        </div>
      </header>

      <div className="flex-1 flex overflow-hidden">
        <div 
          style={{ width: `${splitterPos}px` }}
          className="flex flex-col border-r border-theme-glass-light overflow-hidden shrink-0 glass-panel z-10"
        >
          {isSearchVisible && (
            <div className="px-4 py-2 glass-card-sm border-b border-theme-glass-light">
              <input 
                type="text" 
                autoFocus
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                placeholder="Filter by computer, user, note..." 
                className="glass-input w-full pl-4 py-2 text-sm text-theme-primary placeholder:text-theme-muted"
              />
            </div>
          )}
          <div className="flex-1 overflow-auto custom-scrollbar bg-theme-glass-panel">
            <table className="glass-table min-w-full">
              <thead>
                <tr>
                  <th>Node</th>
                  <th>Operator</th>
                  <th className="w-24">Time</th>
                </tr>
              </thead>
              <tbody className="text-[11px] font-medium">
                {filteredScreenshots.length === 0 ? (
                  <tr><td colSpan="3" className="py-12 text-center text-theme-muted uppercase font-black text-[9px] tracking-widest italic border-none">No captures</td></tr>
                ) : (
                  [...filteredScreenshots].reverse().map((s) => (
                    <tr 
                      key={s.s_screen_id} 
                      onClick={() => setSelectedId(s.s_screen_id)}
                      onContextMenu={(e) => handleContextMenu(e, s)}
                      onDoubleClick={() => setIsPreviewOpen(true)}
                      className={cn(
                        "transition-colors group h-8 cursor-default border-b border-theme-glass-light hover:bg-theme-glass",
                        selectedId === s.s_screen_id && "bg-theme-accent/10"
                      )}
                    >
                      <td className="text-theme-accent font-black font-mono truncate max-w-[120px]">{s.s_computer}</td>
                      <td className="text-theme-secondary truncate max-w-[100px]">{s.s_user}</td>
                      <td className="text-theme-muted font-mono text-[9px]">
                        {new Date(s.s_date * 1000).toLocaleTimeString([], { hour12: false, hour: '2-digit', minute: '2-digit' })}
                      </td>
                    </tr>
                  ))
                )}
              </tbody>
            </table>
          </div>
        </div>

        <div 
          className="w-1 bg-theme-glass-light hover:bg-theme-accent/50 cursor-col-resize transition-colors z-20 flex items-center justify-center group"
          onMouseDown={() => {
            isResizing.current = true;
            document.body.style.cursor = 'col-resize';
          }}
        >
          <div className="h-8 w-0.5 bg-theme-glass rounded-full group-hover:bg-theme-accent transition-colors" />
        </div>

        <div className="flex-1 bg-theme-glass-panel flex flex-col relative overflow-hidden group/canvas">
          {/* Canvas Toolbar overlay */}
          <AnimatePresence>
            {selectedScreen && (
              <motion.div 
                initial={{ y: -40, opacity: 0 }}
                animate={{ y: 0, opacity: 1 }}
                exit={{ y: -40, opacity: 0 }}
                className="absolute top-4 left-1/2 -translate-x-1/2 z-30 flex items-center space-x-1 p-1.5 bg-theme-glass-panel/90 backdrop-blur-md border border-theme-glass-light rounded-xl shadow-glow opacity-0 group-hover/canvas:opacity-100 transition-opacity duration-300"
              >
                <button onClick={() => handleDownload(selectedScreen)} className="p-1.5 hover:bg-theme-accent/20 text-theme-muted hover:text-theme-accent rounded-lg transition-colors" title="Download PNG"><Download size={16}/></button>
                <div className="w-px h-4 bg-theme-glass-light mx-1" />
                <button onClick={() => setIsPreviewOpen(true)} className="p-1.5 hover:bg-theme-accent-secondary/20 text-theme-muted hover:text-theme-accent-secondary rounded-lg transition-colors" title="Full Screen"><Maximize2 size={16}/></button>
                <div className="w-px h-4 bg-theme-glass-light mx-1" />
                <button onClick={() => handleSetNote(selectedScreen)} className="p-1.5 hover:bg-theme-primary/10 text-theme-muted hover:text-theme-primary rounded-lg transition-colors" title="Edit Note"><Edit3 size={16}/></button>
                <div className="w-px h-4 bg-theme-glass-light mx-1" />
                <button onClick={() => handleDelete(selectedScreen.screen_id)} className="p-1.5 hover:bg-theme-danger/20 text-theme-muted hover:text-theme-danger rounded-lg transition-colors" title="Delete"><Trash2 size={16}/></button>
              </motion.div>
            )}
          </AnimatePresence>

          <div className="flex-1 flex items-center justify-center p-8 overflow-auto custom-scrollbar">
            {selectedScreen ? (
              <motion.div 
                key={selectedScreen.s_screen_id}
                initial={{ opacity: 0, scale: 0.98 }}
                animate={{ opacity: 1, scale: 1 }}
                className="relative"
              >
                <img 
                  src={selectedScreen.s_content ? `data:image/png;base64,${selectedScreen.s_content}` : '/placeholder-screen.png'} 
                  alt="Capture Output"
                  className="max-w-full max-h-full shadow-glow rounded-lg border border-theme-glass-light select-text"
                />
              </motion.div>
            ) : (
              <div className="flex flex-col items-center justify-center opacity-10 space-y-4">
                <ImageIcon size={120} className="text-theme-muted" />
                <p className="text-sm font-black uppercase tracking-[0.3em] text-theme-muted">Select Telemetry Capture</p>
              </div>
            )}
          </div>

          {selectedScreen && (
            <div className="px-3 py-2 bg-theme-glass border-t border-theme-glass-light flex items-center justify-between text-[10px] font-black text-theme-muted uppercase tracking-widest shrink-0">
              <div className="flex items-center space-x-4">
                <span className="text-theme-accent">Capture Info:</span>
                <div className="flex items-center space-x-2 bg-theme-glass-panel px-3 py-1 rounded-lg border border-theme-glass-light">
                  <span className="text-theme-secondary">{selectedScreen.s_computer}</span>
                  <span className="text-theme-muted">/</span>
                  <span className="text-theme-secondary">{selectedScreen.s_username}</span>
                </div>
                <span className="text-theme-muted font-mono text-[9px] opacity-60">{selectedScreen.s_screen_id}</span>
              </div>
              <div className="flex items-center space-x-3">
                <div className="flex items-center space-x-2 bg-theme-glass-panel px-3 py-1 rounded-lg border border-theme-glass-light">
                  <Edit3 size={10} className="text-theme-accent" />
                  <span className="text-theme-primary italic normal-case font-medium">{selectedScreen.s_note || 'NO_ARTIFACT_NOTE'}</span>
                </div>
              </div>
            </div>
          )}
        </div>
      </div>

      <AnimatePresence>
        {isPreviewOpen && selectedScreen && (
          <div 
            className="fixed inset-0 z-[200] bg-theme-glass-panel/90 backdrop-blur-md flex flex-col p-4"
            onClick={() => setIsPreviewOpen(false)}
          >
            <div className="flex items-center justify-between px-6 py-4 border-b border-theme-glass-light bg-theme-glass-panel/80 rounded-t-2xl">
              <div className="flex items-center space-x-4">
                <div className="p-2 bg-theme-glass-panel border border-theme-glass-light rounded-lg">
                  <FileImage className="text-theme-accent" size={20} />
                </div>
                <div className="text-left">
                  <p className="text-[10px] font-black uppercase tracking-widest text-theme-muted mb-0.5">Telemetry Capture Preview</p>
                  <p className="text-sm font-mono font-bold text-theme-primary">
                    {selectedScreen.s_computer} \ {selectedScreen.s_username}
                  </p>
                </div>
              </div>
              <button 
                onClick={() => setIsPreviewOpen(false)} 
                className="p-2 text-theme-muted hover:text-theme-primary hover:bg-theme-glass rounded-xl transition-all"
              >
                <X size={24} />
              </button>
            </div>
            <div className="flex-1 flex items-center justify-center p-8 overflow-auto">
              <motion.img 
                initial={{ scale: 0.9, opacity: 0 }}
                animate={{ scale: 1, opacity: 1 }}
                src={`data:image/png;base64,${selectedScreen.s_content}`}
                className="max-w-full max-h-full object-contain shadow-2xl rounded-lg cursor-zoom-out"
              />
            </div>
          </div>
        )}
      </AnimatePresence>

      {menu && <ContextMenu {...menu} onClose={() => setMenu(null)} />}
    </div>
  );
};

export default ScreenshotsList;
