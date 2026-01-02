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
        return 'text-[#E0E0E0]'; // COLOR_ConsoleWhite
      case 2: // EVENT_CLIENT_DISCONNECT
        return 'text-[#808080]'; // COLOR_Gray
      case 3: // EVENT_LISTENER_START
      case 4: // EVENT_LISTENER_STOP
        return 'text-[#FFA500]'; // COLOR_BrightOrange
      case 5: // EVENT_AGENT_NEW
        return 'text-[#39FF14] font-bold'; // COLOR_NeonGreen
      case 6: // EVENT_TUNNEL_START
      case 7: // EVENT_TUNNEL_STOP
        return 'text-[#FDFD96]'; // COLOR_PastelYellow
      default:
        return 'text-[#E0E0E0]';
    }
  };

  return (
    <div className="flex flex-col h-full w-full bg-dark-900 select-none overflow-hidden">
      {/* 1. Header */}
      <div className="flex items-center justify-between px-4 py-2 bg-dark-800 border-b border-dark-700 shrink-0 select-none">
        <div className="flex items-center space-x-3">
          <div className="flex items-center space-x-2 px-2 py-0.5 rounded bg-orange-500/10 border border-orange-500/20">
            <ScrollText className="w-3.5 h-3.5 text-orange-500" />
            <span className="text-[10px] font-black uppercase tracking-widest text-orange-500">Event Logs</span>
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

        <div className="flex items-center space-x-2">
          <button 
            onClick={() => setLogs([])}
            className="p-1.5 rounded hover:bg-dark-700 text-gray-400 hover:text-accent-danger transition-all"
            title="Clear Logs (Ctrl+L)"
          >
            <Trash2 size={14} />
          </button>
          <button className="p-1.5 rounded hover:bg-dark-700 text-gray-400 hover:text-white transition-all" title="Export Logs">
            <Download size={14} />
          </button>
        </div>
      </div>

      {/* 2. Search Bar */}
      {isSearchVisible && (
        <div className="flex items-center px-4 py-2 bg-dark-800/50 border-b border-dark-700 animate-in slide-in-from-top-2 duration-200 shrink-0 select-none">
          <div className="flex items-center space-x-2 mr-4 text-gray-500">
            <button 
              onClick={handlePrevSearch}
              className="p-0.5 hover:text-white transition-colors"
            >
              <ChevronLeft size={14}/>
            </button>
            <button 
              onClick={handleNextSearch}
              className="p-0.5 hover:text-white transition-colors"
            >
              <ChevronRight size={14}/>
            </button>
            <span className="text-[10px] font-bold min-w-[40px] text-center">
              {searchResults.length > 0 ? `${currentSearchIndex + 1} of ${searchResults.length}` : '0 of 0'}
            </span>
          </div>
          <div className="relative flex-1 max-w-md">
            <input 
              type="text" 
              autoFocus
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Find in logs..." 
              className="w-full bg-dark-950/50 border border-dark-600 rounded px-3 py-1 text-[11px] text-gray-300 outline-none focus:border-accent-primary/50 placeholder:text-gray-700"
            />
            {searchQuery && (
              <button onClick={() => setSearchQuery('')} className="absolute right-2 top-1/2 -translate-y-1/2 text-gray-600 hover:text-gray-400">
                <X size={12} />
              </button>
            )}
          </div>
        </div>
      )}

      {/* 3. Logs Content */}
      <div 
        ref={scrollRef}
        className="flex-1 overflow-auto p-4 custom-scrollbar bg-[#0a0a0a]"
      >
        <div className="space-y-0.5">
          {displayedLogs.map((log, i) => {
            const isSelected = searchResults[currentSearchIndex] === i;
            return (
              <div 
                key={i} 
                className={cn(
                  "flex items-start space-x-2 group log-entry transition-colors",
                  isSelected && "bg-accent-primary/20 ring-1 ring-accent-primary/30"
                )}
              >
                <span className="text-gray-600 shrink-0">
                  [{new Date(log.time * 1000).toLocaleTimeString([], { hour12: false })}]
                </span>
                <span className="text-gray-500 shrink-0">-&gt;</span>
                <span className={cn("break-all whitespace-pre-wrap", getLogColor(log.type))}>
                  {log.content || log.message}
                </span>
              </div>
            );
          })}
          {displayedLogs.length === 0 && (
            <div className="py-20 flex flex-col items-center justify-center opacity-10 select-none">
              <ScrollText size={64} />
              <p className="mt-4 text-sm font-bold tracking-widest uppercase">Console Empty</p>
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

export default LogsList;
