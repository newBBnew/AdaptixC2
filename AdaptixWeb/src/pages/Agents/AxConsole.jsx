import React, { useState, useRef, useEffect, useCallback } from 'react';
import { Search, X, ChevronUp, ChevronDown, RotateCcw, History } from 'lucide-react';
import { cn } from '../../utils/cn';
import { useAgents } from '../../context/AgentContext';

const AxConsole = () => {
  const { axEngine, axCommands } = useAgents();
  const [input, setInput] = useState('');
  const [output, setOutput] = useState([]);
  const [history, setHistory] = useState([]);
  const [historyIndex, setHistoryIndex] = useState(-1);
  const [searchQuery, setSearchQuery] = useState('');
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const [showHistory, setShowHistory] = useState(false);
  
  const inputRef = useRef(null);
  const outputRef = useRef(null);

  // Auto-scroll to bottom
  useEffect(() => {
    if (outputRef.current) {
      outputRef.current.scrollTop = outputRef.current.scrollHeight;
    }
  }, [output]);

  // Keyboard shortcuts
  useEffect(() => {
    const handleKeyDown = (e) => {
      if (e.ctrlKey && e.key === 'f') {
        e.preventDefault();
        setIsSearchVisible(prev => !prev);
      }
      if (e.ctrlKey && e.key === 'l') {
        e.preventDefault();
        setOutput([]);
      }
      if (e.ctrlKey && e.key === 'h') {
        e.preventDefault();
        setShowHistory(prev => !prev);
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, []);

  const appendOutput = useCallback((text, type = 'info') => {
    const timestamp = new Date().toLocaleTimeString();
    setOutput(prev => [...prev, { timestamp, text, type }]);
  }, []);

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
      appendOutput('  plugins           - List registered plugins', 'info');
      appendOutput('  reload            - Reload Extension-Kit scripts', 'info');
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

    if (cmdName === 'plugins') {
      const plugins = axEngine?.plugins || [];
      appendOutput(`Registered plugins (${plugins.length}):`, 'info');
      plugins.forEach(p => {
        appendOutput(`  [${p.category}] ${p.command}`, 'info');
      });
      return;
    }

    if (cmdName === 'reload') {
      appendOutput('Reloading Extension-Kit scripts...', 'info');
      try {
        axEngine.loadedScripts.clear();
        axEngine.commands.clear();
        axEngine.plugins = [];
        await axEngine.loadMainScript();
        appendOutput(`Loaded ${axEngine.commands.size} commands`, 'success');
      } catch (err) {
        appendOutput(`Error: ${err.message}`, 'error');
      }
      return;
    }

    // Try to execute as JavaScript
    try {
      const result = eval(cmd);
      if (result !== undefined) {
        appendOutput(String(result), 'success');
      }
    } catch (err) {
      appendOutput(`Error: ${err.message}`, 'error');
    }
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
      case 'command': return 'text-accent-primary font-bold';
      case 'success': return 'text-green-400';
      case 'error': return 'text-red-400';
      case 'warning': return 'text-yellow-400';
      default: return 'text-gray-300';
    }
  };

  const filteredOutput = searchQuery 
    ? output.filter(line => line.text.toLowerCase().includes(searchQuery.toLowerCase()))
    : output;

  return (
    <div className="flex flex-col h-full bg-dark-900">
      {/* Search bar */}
      {isSearchVisible && (
        <div className="flex items-center px-3 py-2 bg-dark-800 border-b border-dark-700 space-x-2">
          <Search className="w-4 h-4 text-gray-500" />
          <input
            type="text"
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="Search..."
            autoFocus
            className="flex-1 bg-transparent text-sm text-white outline-none"
          />
          <span className="text-xs text-gray-500">
            {filteredOutput.length} / {output.length}
          </span>
          <button onClick={() => setIsSearchVisible(false)} className="p-1 hover:bg-dark-700 rounded">
            <X className="w-3 h-3 text-gray-500" />
          </button>
        </div>
      )}

      {/* History panel */}
      {showHistory && (
        <div className="max-h-40 overflow-auto bg-dark-800 border-b border-dark-700">
          <div className="px-3 py-1.5 text-[10px] font-bold text-gray-500 uppercase tracking-widest border-b border-dark-700 flex items-center justify-between">
            <span>Command History</span>
            <button onClick={() => setShowHistory(false)} className="p-0.5 hover:bg-dark-700 rounded">
              <X className="w-3 h-3" />
            </button>
          </div>
          {history.length === 0 ? (
            <div className="px-3 py-2 text-xs text-gray-500">No history</div>
          ) : (
            history.slice().reverse().map((cmd, i) => (
              <button
                key={i}
                onClick={() => { setInput(cmd); setShowHistory(false); inputRef.current?.focus(); }}
                className="w-full px-3 py-1.5 text-left text-xs font-mono text-gray-300 hover:bg-dark-700 truncate"
              >
                {cmd}
              </button>
            ))
          )}
        </div>
      )}

      {/* Output area */}
      <div 
        ref={outputRef}
        className="flex-1 overflow-auto p-3 font-mono text-xs space-y-0.5 scrollbar-thin"
      >
        {filteredOutput.length === 0 ? (
          <div className="text-gray-500 text-center py-10">
            <p className="mb-2">AxScript Console</p>
            <p className="text-[10px]">Type 'help' for available commands</p>
          </div>
        ) : (
          filteredOutput.map((line, i) => (
            <div key={i} className="flex">
              <span className="text-gray-600 mr-2 select-none">[{line.timestamp}]</span>
              <span className={getOutputClass(line.type)}>{line.text}</span>
            </div>
          ))
        )}
      </div>

      {/* Input area */}
      <div className="flex items-center px-3 py-2 bg-dark-800 border-t border-dark-700">
        <span className="text-accent-primary mr-2 font-bold">ax&gt;</span>
        <input
          ref={inputRef}
          type="text"
          value={input}
          onChange={(e) => setInput(e.target.value)}
          onKeyDown={handleKeyDown}
          placeholder="Enter command..."
          className="flex-1 bg-transparent text-sm text-white font-mono outline-none"
        />
        <div className="flex items-center space-x-1 ml-2">
          <button 
            onClick={() => setShowHistory(!showHistory)}
            className={cn("p-1.5 rounded transition-colors", showHistory ? "bg-accent-primary/20 text-accent-primary" : "hover:bg-dark-700 text-gray-500")}
            title="History (Ctrl+H)"
          >
            <History className="w-3.5 h-3.5" />
          </button>
          <button 
            onClick={() => setIsSearchVisible(!isSearchVisible)}
            className={cn("p-1.5 rounded transition-colors", isSearchVisible ? "bg-accent-primary/20 text-accent-primary" : "hover:bg-dark-700 text-gray-500")}
            title="Search (Ctrl+F)"
          >
            <Search className="w-3.5 h-3.5" />
          </button>
          <button 
            onClick={() => setOutput([])}
            className="p-1.5 hover:bg-dark-700 rounded text-gray-500 transition-colors"
            title="Clear (Ctrl+L)"
          >
            <RotateCcw className="w-3.5 h-3.5" />
          </button>
        </div>
      </div>
    </div>
  );
};

export default AxConsole;
