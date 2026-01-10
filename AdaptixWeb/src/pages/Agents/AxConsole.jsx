import React, { useState, useRef, useEffect, useCallback } from 'react';
import { Search, X, ChevronUp, ChevronDown, RotateCcw, History, Code2, ChevronRight } from 'lucide-react';
import { cn } from '../../utils/cn';
import { useAgents } from '../../context/AgentContext';

const AxConsole = () => {
  const { axCommands, axStats, reloadScripts } = useAgents();
  const [input, setInput] = useState('');
  const [output, setOutput] = useState([]);
  const [history, setHistory] = useState([]);
  const [historyIndex, setHistoryIndex] = useState(-1);
  const [searchQuery, setSearchQuery] = useState('');
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const [showHistory, setShowHistory] = useState(false);
  
  const inputRef = useRef(null);
  const outputRef = useRef(null);
  const searchInputRef = useRef(null);

  // ... (useEffect for auto-scroll and shortcuts remain same)

  const processInput = async () => {
    const cmd = input.trim();
    if (!cmd) return;

    // Add to history
    setHistory(prev => [...prev, cmd]);
    setHistoryIndex(-1);
    setInput('');

    // Echo command
    appendOutput(`> ${cmd}`, 'command');

    // Parse command
    const parts = cmd.split(/\s+/);
    const cmdName = parts[0];

    // Built-in commands
    if (cmdName === 'help') {
      appendOutput('Available commands:', 'info');
      appendOutput('  help              - Show this help', 'info');
      appendOutput('  clear             - Clear console', 'info');
      appendOutput('  commands          - List registered commands', 'info');
      appendOutput('  reload            - Reload Extension-Kit scripts on Gateway', 'info');
      appendOutput('', 'info');
      appendOutput('Keyboard shortcuts:', 'info');
      appendOutput('  Ctrl+F            - Toggle search', 'info');
      appendOutput('  Ctrl+L            - Clear console', 'info');
      appendOutput('  Ctrl+H            - Show history', 'info');
      appendOutput('  Up/Down           - Navigate history', 'info');
      return;
    }

    if (cmdName === 'clear') {
      setOutput([]);
      return;
    }

    if (cmdName === 'commands') {
      appendOutput(`Registered commands (${axCommands.length}):`, 'info');
      axCommands.forEach(c => {
        appendOutput(`  ${c.name.padEnd(20)} - ${c.description}`, 'info');
      });
      return;
    }

    if (cmdName === 'reload') {
      appendOutput('Reloading Extension-Kit scripts on Gateway...', 'info');
      try {
        await reloadScripts();
        appendOutput(`Successfully reloaded extensions. Total commands: ${axCommands.length}`, 'success');
      } catch (err) {
        appendOutput(`Error: ${err.message}`, 'error');
      }
      return;
    }

    // Direct JS execution is disabled in Web Console for security;
    // Scripts should be loaded as Extensions on the Gateway.
    appendOutput(`Unknown command: ${cmdName}. Only built-in UI commands are supported in this console. Use Agent console for tactical operations.`, 'error');
  };

  const handleKeyDown = (e) => {
    if (e.key === 'Enter') {
      processInput();
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      if (history.length > 0) {
        const newIndex = historyIndex < history.length - 1 ? historyIndex + 1 : historyIndex;
        setHistoryIndex(newIndex);
        setInput(history[history.length - 1 - newIndex] || '');
      }
    } else if (e.key === 'ArrowDown') {
      e.preventDefault();
      if (historyIndex > 0) {
        const newIndex = historyIndex - 1;
        setHistoryIndex(newIndex);
        setInput(history[history.length - 1 - newIndex] || '');
      } else if (historyIndex === 0) {
        setHistoryIndex(-1);
        setInput('');
      }
    }
  };

  const getOutputClass = (type) => {
    switch (type) {
      case 'command': return 'text-theme-primary font-black opacity-80';
      case 'success': return 'text-theme-success font-bold'; 
      case 'error': return 'text-theme-danger font-bold'; 
      case 'warning': return 'text-theme-accent-secondary font-bold';
      default: return 'text-theme-secondary';
    }
  };

  const filteredOutput = searchQuery 
    ? output.filter(line => line.text.toLowerCase().includes(searchQuery.toLowerCase()))
    : output;

  return (
    <div className="flex flex-col h-full bg-theme-glass-panel select-none overflow-hidden">
      {/* Search bar */}
      {isSearchVisible && (
        <div className="flex items-center px-4 py-2 bg-theme-glass border-b border-theme-glass-light space-x-4 animate-in slide-in-from-top-1 duration-200 z-20">
          <Search className="w-4 h-4 text-theme-accent" />
          <input
            ref={searchInputRef}
            type="text"
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="SEARCH_AX_BUFFER..."
            autoFocus
            className="flex-1 glass-input text-xs py-1.5"
            onKeyDown={(e) => {
              if (e.key === 'Escape') setIsSearchVisible(false);
            }}
          />
          <div className="flex items-center space-x-3">
            <span className="text-[10px] text-theme-muted font-mono bg-theme-glass-panel px-2 py-0.5 rounded-lg border border-theme-glass-light shadow-sm">
              {filteredOutput.length} / {output.length}
            </span>
            <button onClick={() => setIsSearchVisible(false)} className="p-1.5 hover:bg-theme-glass-panel text-theme-muted hover:text-theme-danger transition-all rounded-lg">
              <X size={16} />
            </button>
          </div>
        </div>
      )}

      {/* History panel */}
      {showHistory && (
        <div className="max-h-64 overflow-hidden bg-theme-glass-panel border-b border-theme-glass-light flex flex-col shadow-glow z-10">
          <div className="px-4 py-2 text-[10px] font-black text-theme-muted uppercase tracking-[0.2em] border-b border-theme-glass-light flex items-center justify-between bg-theme-glass">
            <div className="flex items-center space-x-2">
              <History size={14} className="text-theme-accent" />
              <span>Script Execution History</span>
            </div>
            <button onClick={() => setShowHistory(false)} className="p-1 hover:bg-theme-glass-panel rounded-lg text-theme-muted hover:text-theme-primary transition-all">
              <X size={16} />
            </button>
          </div>
          <div className="overflow-y-auto custom-scrollbar flex-1 bg-theme-glass-panel/50">
            {history.length === 0 ? (
              <div className="px-6 py-10 text-[10px] text-theme-muted font-black uppercase tracking-widest text-center italic opacity-40">No artifacts in session history</div>
            ) : (
              [...history].reverse().map((cmd, i) => (
                <button
                  key={i}
                  onClick={() => { setInput(cmd); setShowHistory(false); inputRef.current?.focus(); }}
                  className="w-full px-6 py-2.5 text-left text-[11px] font-mono text-theme-secondary hover:bg-theme-glass hover:text-theme-primary truncate transition-all border-b border-theme-glass-light/30 last:border-0 group flex items-center space-x-4"
                >
                  <span className="text-theme-muted font-black text-[9px] w-6 opacity-40">{(history.length - i).toString().padStart(2, '0')}</span>
                  <span className="flex-1 truncate">{cmd}</span>
                </button>
              ))
            )}
          </div>
        </div>
      )}

      {/* Output area */}
      <div 
        ref={outputRef}
        className="flex-1 overflow-auto p-6 font-mono text-[12px] space-y-1 custom-scrollbar bg-theme-glass-panel/30 select-text leading-relaxed"
      >
        {filteredOutput.length === 0 ? (
          <div className="h-full flex flex-col items-center justify-center text-theme-muted opacity-20 select-none space-y-6">
            <Code2 size={120} strokeWidth={1} />
            <div className="text-center">
              <p className="text-sm font-black uppercase tracking-[0.5em]">AxScript Engine</p>
              <p className="text-[10px] mt-2 font-bold tracking-widest">READY_FOR_ORCHESTRATION</p>
            </div>
          </div>
        ) : (
          filteredOutput.map((line, i) => (
            <div key={i} className="flex items-start group hover:bg-theme-glass/10 transition-colors rounded-lg px-2 -mx-2 py-0.5">
              <span className="text-theme-muted text-[10px] mr-4 select-none shrink-0 mt-0.5 font-bold opacity-40">[{line.timestamp}]</span>
              <span className={cn("break-all whitespace-pre-wrap flex-1 min-w-0", getOutputClass(line.type))}>
                {line.text}
              </span>
            </div>
          ))
        )}
      </div>

      {/* Input area */}
      <div className="flex flex-col bg-theme-glass border-t border-theme-glass-light shadow-glow-sm">
        <div className="flex items-center px-3 py-2 space-x-4">
          <div className="flex items-center space-x-2 ml-2 shrink-0">
            <span className="text-theme-accent font-black text-[11px] uppercase tracking-widest">ax_engine</span>
            <ChevronRight size={16} className="text-theme-muted opacity-60" />
          </div>
          <input
            ref={inputRef}
            type="text"
            value={input}
            onChange={(e) => setInput(e.target.value)}
            onKeyDown={handleKeyDown}
            placeholder="INVOKE_EXTENSION_SCRIPT..."
            className="flex-1 bg-theme-glass-panel/40 border border-theme-glass-light rounded-xl px-4 py-2 text-theme-primary font-mono text-[12px] placeholder:text-theme-muted/40 focus:border-theme-accent/50 focus:bg-theme-glass-panel/60 outline-none transition-all shadow-inner"
          />
          <div className="flex items-center space-x-2 pr-2 shrink-0">
            <button 
              onClick={() => setShowHistory(!showHistory)}
              className={cn("p-2 rounded-xl transition-all border shadow-sm", showHistory ? "bg-theme-accent/20 text-theme-accent border-theme-accent/40" : "bg-theme-glass-panel border-theme-glass-light text-theme-muted hover:text-theme-primary")}
              title="HISTORY (Ctrl+H)"
            >
              <History size={16} />
            </button>
            <button 
              onClick={() => setIsSearchVisible(!isSearchVisible)}
              className={cn("p-2 rounded-xl transition-all border shadow-sm", isSearchVisible ? "bg-theme-accent/20 text-theme-accent border-theme-accent/40" : "bg-theme-glass-panel border-theme-glass-light text-theme-muted hover:text-theme-primary")}
              title="FILTER (Ctrl+F)"
            >
              <Search size={16} />
            </button>
            <div className="w-px h-5 bg-theme-glass-light mx-1" />
            <button 
              onClick={() => setOutput([])}
              className="p-2 bg-theme-glass-panel border border-theme-glass-light rounded-xl text-theme-muted hover:text-theme-danger transition-all shadow-sm"
              title="PURGE_BUFFER (Ctrl+L)"
            >
              <RotateCcw size={16} />
            </button>
          </div>
        </div>
        <div className="px-6 py-1.5 bg-theme-glass-panel flex items-center justify-between text-[9px] font-black text-theme-muted uppercase tracking-[0.15em] shrink-0 border-t border-theme-glass-light/30">
          <div className="flex items-center space-x-6">
            <div className="flex items-center space-x-2">
              <span className="opacity-60">COMMANDS_LOADED:</span>
              <span className="text-theme-accent font-mono">{axCommands.length}</span>
            </div>
            <div className="flex items-center space-x-2">
              <span className="opacity-60">PLUGINS_ACTIVE:</span>
              <span className="text-theme-accent-secondary font-mono">{axPlugins.length}</span>
            </div>
          </div>
          <div className="flex items-center space-x-3">
            <span>ENGINE_SYNCHRONIZED</span>
            <div className="w-2 h-2 rounded-full bg-theme-success shadow-glow-sm animate-pulse" />
          </div>
        </div>
      </div>
    </div>
  );
};

export default AxConsole;
