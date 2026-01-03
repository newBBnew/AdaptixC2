import React, { useState, useEffect, useRef } from 'react';
import { 
  ScrollText, 
  Search, 
  ChevronLeft, 
  ChevronRight, 
  X, 
  Trash2,
  Download
} from 'lucide-react';
import { cn } from '../../utils/cn';
import { useSocket } from '../../context/SocketContext';
import { useAgents } from '../../context/AgentContext';

const LogsList = () => {
  const { logs } = useAgents();
  const [searchQuery, setSearchQuery] = useState('');
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const [searchResults, setSearchResults] = useState([]);
  const [currentSearchIndex, setCurrentIndex] = useState(-1);
  const scrollRef = useRef(null);

  const displayedLogs = logs;

  useEffect(() => {
    if (searchQuery.trim()) {
      const results = [];
      logs.forEach((log, index) => {
        if (log.message.toLowerCase().includes(searchQuery.toLowerCase())) {
          results.push(index);
        }
      });
      setSearchResults(results);
      setCurrentIndex(results.length > 0 ? 0 : -1);
    } else {
      setSearchResults([]);
      setCurrentIndex(-1);
    }
  }, [searchQuery, logs]);

  const handleNextSearch = () => {
    if (searchResults.length > 0) {
      setCurrentIndex((prev) => (prev + 1) % searchResults.length);
    }
  };

  const handlePrevSearch = () => {
    if (searchResults.length > 0) {
      setCurrentIndex((prev) => (prev - 1 + searchResults.length) % searchResults.length);
    }
  };

  useEffect(() => {
    if (currentSearchIndex !== -1 && scrollRef.current) {
      const logElements = scrollRef.current.querySelectorAll('.log-entry');
      const targetIndex = searchResults[currentSearchIndex];
      if (logElements[targetIndex]) {
        logElements[targetIndex].scrollIntoView({ behavior: 'smooth', block: 'center' });
      }
    }
  }, [currentSearchIndex, searchResults]);

  useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [displayedLogs]);

  useEffect(() => {
    const handleKeyDown = (e) => {
      if ((e.ctrlKey || e.metaKey) && e.key === 'f') {
        e.preventDefault();
        setIsSearchVisible(prev => !prev);
      }
      if ((e.ctrlKey || e.metaKey) && e.key === 'l') {
        e.preventDefault();
        // Note: Clear is visual only since logs come from server
        if (scrollRef.current) {
          scrollRef.current.scrollTop = 0;
        }
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, []);

  const getLogColor = (type) => {
    switch (type) {
      case 1: // EVENT_CLIENT_CONNECT
        return 'text-theme-primary opacity-90';
      case 2: // EVENT_CLIENT_DISCONNECT
        return 'text-theme-muted italic';
      case 3: // EVENT_LISTENER_START
      case 4: // EVENT_LISTENER_STOP
        return 'text-theme-accent font-bold';
      case 5: // EVENT_AGENT_NEW
        return 'text-theme-success font-black tracking-tight';
      case 6: // EVENT_TUNNEL_START
      case 7: // EVENT_TUNNEL_STOP
        return 'text-theme-accent-secondary';
      default:
        return 'text-theme-primary';
    }
  };

  return (
    <div className="flex flex-col h-full w-full select-none overflow-hidden" onClick={() => setMenu(null)}>
      {/* 1. Header */}
      <div className="flex items-center justify-between px-3 py-2 glass-card-sm border-b border-theme-glass-light shrink-0">
        <div className="flex items-center space-x-3">
          <button 
            onClick={() => setIsSearchVisible(!isSearchVisible)}
            className={cn(
              "p-2 rounded-lg transition-colors",
              isSearchVisible ? "bg-theme-hover text-theme-accent" : "text-theme-muted hover:text-theme-primary hover:bg-theme-hover"
            )}
            title="Toggle Search (Ctrl+F)"
          >
            <Search className="w-4 h-4" />
          </button>
        </div>

        <div className="flex items-center space-x-2 pr-1">
          <button 
            className="p-2 glass-btn text-theme-muted hover:text-theme-danger transition-all"
            title="Clear Buffer (Ctrl+L)"
          >
            <Trash2 size={16} />
          </button>
          <button className="p-2 glass-btn text-theme-muted hover:text-theme-accent transition-all" title="Export Logs">
            <Download size={16} />
          </button>
        </div>
      </div>

      {/* 2. Search Bar */}
      {isSearchVisible && (
        <div className="flex items-center px-4 py-2 glass-card-sm border-b border-theme-glass-light shrink-0 space-x-4">
        <div className="flex items-center space-x-2 text-theme-muted glass-btn px-3 py-1.5 rounded-lg">
          <button 
            onClick={handlePrevSearch}
            className="p-1 hover:text-theme-accent transition-colors"
          >
            <ChevronLeft size={16}/>
          </button>
          <button 
            onClick={handleNextSearch}
            className="p-1 hover:text-theme-accent transition-colors"
          >
            <ChevronRight size={16}/>
          </button>
          <div className="w-px h-4 bg-theme-glass-light mx-1" />
          <span className="text-xs font-medium min-w-[50px] text-center">
            {searchResults.length > 0 ? `${currentSearchIndex + 1}/${searchResults.length}` : '0/0'}
          </span>
        </div>
        <div className="relative flex-1 max-w-md">
          <input 
            type="text" 
            autoFocus
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="Search logs..." 
            className="glass-input w-full pl-3 py-2 text-sm text-theme-primary placeholder:text-theme-muted"
          />
          {searchQuery && (
            <button onClick={() => setSearchQuery('')} className="absolute right-3 top-1/2 -translate-y-1/2 text-theme-muted hover:text-theme-danger transition-colors">
              <X size={16} />
            </button>
          )}
        </div>
      </div>
      )}

      {/* 3. Logs Content */}
      <div 
        ref={scrollRef}
        className="flex-1 overflow-auto p-4 custom-scrollbar glass-panel font-mono text-[12px] leading-relaxed text-theme-primary"
      >
        <div className="space-y-1">
          {displayedLogs.map((log, i) => {
            const isSelected = searchResults[currentSearchIndex] === i;
            return (
              <div 
                key={i} 
                className={cn(
                  "flex items-start space-x-3 group log-entry transition-all py-1 px-2 -mx-2 rounded-lg",
                  isSelected ? "bg-theme-glass ring-1 ring-theme-accent/30" : "hover:bg-theme-hover"
                )}
              >
                <span className="text-theme-muted shrink-0 select-none font-medium text-[11px] mt-0.5">
                  [{new Date(log.time * 1000).toLocaleTimeString([], { hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' })}]
                </span>
                <span className="text-theme-muted shrink-0 opacity-50 select-none">::</span>
                <div className="flex-1 min-w-0">
                  <span className={cn("break-all whitespace-pre-wrap", getLogColor(log.type))}>
                    {log.content || log.message}
                  </span>
                </div>
              </div>
            );
          })}
          {displayedLogs.length === 0 && (
            <div className="flex flex-col items-center justify-center h-full space-y-4 opacity-30 py-20">
              <ScrollText size={48} className="text-theme-muted" />
              <p className="text-[10px] font-black tracking-[0.2em] uppercase text-theme-muted">Awaiting telemetry stream...</p>
            </div>
          )}
        </div>
      </div>

      {/* 4. Footer info */}
      <div className="px-4 py-1 glass-card-sm border-t border-theme-glass-light flex items-center justify-between shrink-0">
        <div className="flex items-center space-x-4 text-[10px] font-black uppercase tracking-widest text-theme-muted">
          <div className="flex items-center space-x-1.5">
            <div className="w-1.5 h-1.5 rounded-full bg-theme-success shadow-glow-sm animate-pulse" />
            <span>Node Stream Active</span>
          </div>
          <div className="w-px h-3 bg-theme-glass-light" />
          <span>Total: {logs.length} entries</span>
        </div>
        <div className="text-[10px] font-mono text-theme-muted opacity-50">
          LOG_VER: 1.0.2
        </div>
      </div>
    </div>
  );
};

export default LogsList;
