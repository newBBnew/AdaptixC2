import React, { useState, useEffect, useRef } from 'react';
import { useAgents } from '../../context/AgentContext';
import { agentApi } from '../../api/agent';
import FileBrowser from './FileBrowser';
import ProcessBrowser from './ProcessBrowser';
import { 
  Terminal, 
  Files, 
  Activity, 
  Info,
  ChevronRight,
  Send,
  AlertCircle,
  Search,
  History,
  X,
  ArrowUp,
  ArrowDown,
  Trash2
} from 'lucide-react';
import { cn } from '../../utils/cn';

const AgentConsole = ({ agent }) => {
  const { setActiveSubTab, consoleHistory, addConsoleLine, agentConfigs } = useAgents();
  const activeSubTab = agent.activeSubTab || 'console';
  const [inputValue, setInputValue] = useState('');
  const [commandHistory, setCommandHistory] = useState([]);
  const [historyIndex, setCommandHistoryIndex] = useState(-1);
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [searchIndex, setSearchIndex] = useState(0);
  const [isHistoryDialogOpen, setIsHistoryDialogOpen] = useState(false);
  const scrollRef = useRef(null);
  const inputRef = useRef(null);
  const searchInputRef = useRef(null);

  // Get available commands for this agent type from metadata
  const availableCommands = agentConfigs[agent.a_name]?.commands || [
    'help', 'shell', 'upload', 'download', 'execute', 'exit', 'sleep', 'jitter', 'pwd', 'ls', 'cd', 'whoami', 'ps', 'kill'
  ];

  // Message type colors (aligned with Qt ConsoleWidget.cpp)
  const getMsgPrefix = (msgType) => {
    switch (msgType) {
      case 1: return { prefix: '[+] ', color: 'text-[#FDFD96]' };    // Success - Yellow
      case 2: return { prefix: '[-] ', color: 'text-[#E32227]' };    // Error - ChiliPepper
      case 5: return { prefix: '[*] ', color: 'text-[#89CFF0]' };    // Info - BabyBlue
      default: return { prefix: '', color: 'text-gray-300' };
    }
  };

  const getMsgColor = (msgType) => {
    switch (msgType) {
      case 1: return 'text-[#39FF14] font-bold'; // NeonGreen (Success)
      case 2: return 'text-[#E32227] font-bold'; // ChiliPepper (Error)
      case 3: return 'text-[#FFA500]';           // BrightOrange (Warning)
      case 4: return 'text-[#A01641] font-bold'; // Berry (Critical)
      case 5: return 'text-[#89CFF0] italic';    // BabyBlue (Info)
      case 6: return 'text-[#FDFD96]';           // PastelYellow (System)
      case 10: return 'text-[#E0E0E0]';          // ConsoleWhite
      case 11: return 'text-[#808080]';          // Gray
      default: return 'text-gray-300';
    }
  };

  const formatTimestamp = (ts) => {
    if (!ts) return '';
    const d = new Date(ts * 1000);
    return d.toLocaleString();
  };

  const history = consoleHistory[agent.a_id] || [
    { type: 'info', content: `Session established with ${agent.a_name || agent.a_id.substring(0,8)}` },
    { type: 'info', content: 'Type "help" for a list of available commands.' }
  ];

  useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [history]);

  const handleSubmit = async (e) => {
    e.preventDefault();
    if (!inputValue.trim()) return;
    
    const cmd = inputValue.trim();
    if (cmd.toLowerCase() === 'clear') {
      addConsoleLine(agent.a_id, { type: 'clear' });
      setInputValue('');
      return;
    }

    setInputValue('');
    setCommandHistory(prev => [cmd, ...prev].slice(0, 50));
    setCommandHistoryIndex(-1);

    // Add locally to history immediately
    addConsoleLine(agent.a_id, { type: 'input', content: cmd });

    try {
      await agentApi.executeCommand({
        name: agent.a_name,
        id: agent.a_id,
        ui: true,
        cmdline: cmd,
        data: "{}",
        ax_hook_id: "",
        ax_handler_id: ""
      });
    } catch (err) {
      console.error('Command execution failed:', err);
      addConsoleLine(agent.a_id, { 
        type: 'output', 
        content: `[-] Error: ${err.response?.data?.message || 'Command failed to send'}`,
        msgType: 2 // Error type
      });
    }
  };

  const handleKeyDown = (e) => {
    if (e.key === 'ArrowUp') {
      e.preventDefault();
      if (historyIndex < commandHistory.length - 1) {
        const nextIndex = historyIndex + 1;
        setCommandHistoryIndex(nextIndex);
        setInputValue(commandHistory[nextIndex]);
      }
    } else if (e.key === 'ArrowDown') {
      e.preventDefault();
      if (historyIndex > 0) {
        const nextIndex = historyIndex - 1;
        setCommandHistoryIndex(nextIndex);
        setInputValue(commandHistory[nextIndex]);
      } else if (historyIndex === 0) {
        setCommandHistoryIndex(-1);
        setInputValue('');
      }
    } else if (e.key === 'Tab') {
      e.preventDefault();
      const currentInput = inputValue.trim().toLowerCase();
      if (!currentInput) return;

      const matches = availableCommands.filter(cmd => cmd.toLowerCase().startsWith(currentInput));
      if (matches.length === 1) {
        setInputValue(matches[0] + ' ');
      } else if (matches.length > 1) {
        // Show possible matches in console
        addConsoleLine(agent.a_id, { 
          type: 'info', 
          content: `Possibilities: ${matches.join(', ')}` 
        });
      }
    } else if ((e.ctrlKey || e.metaKey) && e.key === 'f') {
      e.preventDefault();
      setIsSearchVisible(prev => !prev);
      setTimeout(() => searchInputRef.current?.focus(), 50);
    } else if ((e.ctrlKey || e.metaKey) && e.key === 'h') {
      e.preventDefault();
      setIsHistoryDialogOpen(true);
    } else if ((e.ctrlKey || e.metaKey) && e.key === 'l') {
      e.preventDefault();
      addConsoleLine(agent.a_id, { type: 'clear' });
    }
  };

  // Search functionality
  const searchMatches = searchQuery ? history.filter(item => 
    item.content?.toLowerCase().includes(searchQuery.toLowerCase())
  ) : [];

  const navigateSearch = (direction) => {
    if (searchMatches.length === 0) return;
    const newIndex = direction === 'up' 
      ? (searchIndex + 1) % searchMatches.length
      : (searchIndex - 1 + searchMatches.length) % searchMatches.length;
    setSearchIndex(newIndex);
  };

  const subTabs = [
    { id: 'console', name: 'Console', icon: Terminal },
    { id: 'files', name: 'File Browser', icon: Files },
    { id: 'procs', name: 'Processes', icon: Activity },
    { id: 'info', name: 'Info', icon: Info },
  ];

  return (
    <div className="flex flex-col h-full bg-[#0a0a0a]">
      {/* Sub Tabs Header */}
      <div className="flex bg-dark-800 border-b border-dark-700 px-4 h-8 items-center">
        {subTabs.map(tab => (
          <button
            key={tab.id}
            onClick={() => setActiveSubTab(agent.a_id, tab.id)}
            className={cn(
              "flex items-center space-x-2 px-3 py-1 text-[10px] font-black uppercase tracking-widest transition-all h-full border-b-2",
              activeSubTab === tab.id 
                ? "text-accent-primary border-accent-primary bg-accent-primary/5" 
                : "text-gray-500 border-transparent hover:text-gray-300 hover:bg-dark-700/50"
            )}
          >
            <tab.icon className="w-3 h-3" />
            <span>{tab.name}</span>
          </button>
        ))}
      </div>

      {/* Tab Content */}
      <div className="flex-1 overflow-hidden flex flex-col">
        {activeSubTab === 'console' && (
          <>
            {/* Search Panel (Ctrl+F) */}
            {isSearchVisible && (
              <div className="flex items-center px-3 py-1.5 bg-dark-800 border-b border-dark-700 space-x-2 animate-in slide-in-from-top-1 duration-150">
                <Search className="w-3.5 h-3.5 text-gray-500" />
                <input
                  ref={searchInputRef}
                  type="text"
                  value={searchQuery}
                  onChange={(e) => { setSearchQuery(e.target.value); setSearchIndex(0); }}
                  placeholder="Search in console..."
                  className="flex-1 bg-dark-950/50 border border-dark-600 rounded px-2 py-0.5 text-[11px] text-gray-300 outline-none focus:border-accent-primary/50"
                  onKeyDown={(e) => {
                    if (e.key === 'Escape') setIsSearchVisible(false);
                    if (e.key === 'Enter') navigateSearch('down');
                  }}
                />
                <span className="text-[10px] text-gray-500">
                  {searchMatches.length > 0 ? `${searchIndex + 1}/${searchMatches.length}` : '0/0'}
                </span>
                <button onClick={() => navigateSearch('up')} className="p-1 hover:bg-dark-700 rounded">
                  <ArrowUp className="w-3 h-3 text-gray-400" />
                </button>
                <button onClick={() => navigateSearch('down')} className="p-1 hover:bg-dark-700 rounded">
                  <ArrowDown className="w-3 h-3 text-gray-400" />
                </button>
                <button onClick={() => setIsSearchVisible(false)} className="p-1 hover:bg-dark-700 rounded">
                  <X className="w-3 h-3 text-gray-400" />
                </button>
              </div>
            )}

            <div 
              ref={scrollRef}
              className="flex-1 overflow-y-auto p-4 font-mono text-[12px] space-y-1 scrollbar-thin select-text"
            >
              {history.map((item, idx) => {
                if (item.type === 'clear') return null;
                const prefixInfo = getMsgPrefix(item.msgType);
                return (
                  <div key={idx} className="flex items-start group">
                    {item.type === 'input' ? (
                      <>
                        {item.timestamp && <span className="text-[#808080] mr-2">[{formatTimestamp(item.timestamp)}]</span>}
                        {item.user && <span className="text-[#808080] mr-1">{item.user}</span>}
                        {item.taskId && <span className="text-[#808080] mr-1">[{item.taskId.substring(0,6)}]</span>}
                        <span className="text-[#808080] underline mr-1">{agent.a_name}</span>
                        <span className="text-[#808080] mr-2">&gt;</span>
                        <span className="whitespace-pre-wrap font-bold text-white">{item.content}</span>
                      </>
                    ) : item.type === 'info' ? (
                      <span className="text-blue-400/80 italic text-[11px]">{item.content}</span>
                    ) : (
                      <>
                        {item.timestamp && <span className="text-[#808080] mr-2">[{formatTimestamp(item.timestamp)}]</span>}
                        {prefixInfo.prefix && <span className={prefixInfo.color}>{prefixInfo.prefix}</span>}
                        <span className={cn("whitespace-pre-wrap break-words", getMsgColor(item.msgType))}>
                          {item.content}
                        </span>
                      </>
                    )}
                  </div>
                );
              })}
            </div>
            
            <form onSubmit={handleSubmit} className="p-2 bg-dark-800/50 border-t border-dark-700 flex items-center space-x-2">
              <span className="text-accent-primary font-mono font-black text-xs ml-2">adaptix&gt;</span>
              <input
                ref={inputRef}
                type="text"
                autoFocus
                className="flex-1 bg-transparent outline-none text-gray-200 font-mono text-[12px]"
                placeholder="Type command..."
                value={inputValue}
                onChange={(e) => setInputValue(e.target.value)}
                onKeyDown={handleKeyDown}
              />
              <button type="submit" className="text-gray-600 hover:text-accent-primary transition-colors pr-2">
                <Send className="w-3.5 h-3.5" />
              </button>
            </form>
          </>
        )}

        {activeSubTab === 'files' && (
          <FileBrowser agent={agent} />
        )}
        
        {activeSubTab === 'procs' && (
          <ProcessBrowser agent={agent} />
        )}

        {activeSubTab === 'info' && (
          <div className="p-6 overflow-auto scrollbar-thin">
            <div className="flex items-center space-x-3 mb-6">
              <div className="p-2 bg-accent-primary/10 rounded-lg">
                <Info className="w-5 h-5 text-accent-primary" />
              </div>
              <h3 className="text-sm font-black text-white uppercase tracking-widest">Beacon Metadata</h3>
            </div>
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-3">
              {Object.entries(agent).filter(([k]) => k.startsWith('a_')).map(([key, value]) => (
                <div key={key} className="bg-dark-800/50 p-3 rounded border border-dark-700 hover:border-dark-600 transition-colors">
                  <p className="text-[9px] uppercase text-gray-500 font-black mb-1 tracking-tighter">{key.replace('a_', '')}</p>
                  <p className="text-[11px] text-gray-200 font-mono truncate">{String(value || 'N/A')}</p>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>

      {/* Command History Dialog (Ctrl+H) */}
      {isHistoryDialogOpen && (
        <div className="fixed inset-0 z-50 bg-black/60 flex items-center justify-center" onClick={() => setIsHistoryDialogOpen(false)}>
          <div className="bg-dark-800 border border-dark-600 rounded-lg shadow-2xl w-96 max-h-96 overflow-hidden" onClick={e => e.stopPropagation()}>
            <div className="flex items-center justify-between px-4 py-3 border-b border-dark-700">
              <div className="flex items-center space-x-2">
                <History className="w-4 h-4 text-accent-primary" />
                <span className="text-sm font-bold text-white">Command History</span>
              </div>
              <button onClick={() => setIsHistoryDialogOpen(false)} className="p-1 hover:bg-dark-700 rounded">
                <X className="w-4 h-4 text-gray-400" />
              </button>
            </div>
            <div className="max-h-64 overflow-y-auto scrollbar-thin">
              {commandHistory.length === 0 ? (
                <p className="p-4 text-center text-gray-500 text-sm italic">No commands in history</p>
              ) : (
                commandHistory.map((cmd, idx) => (
                  <div 
                    key={idx}
                    className="px-4 py-2 hover:bg-dark-700 cursor-pointer border-b border-dark-700/50 flex items-center justify-between group"
                    onClick={() => { setInputValue(cmd); setIsHistoryDialogOpen(false); inputRef.current?.focus(); }}
                  >
                    <span className="text-[11px] font-mono text-gray-300 truncate">{cmd}</span>
                    <button 
                      onClick={(e) => { e.stopPropagation(); setCommandHistory(prev => prev.filter((_, i) => i !== idx)); }}
                      className="opacity-0 group-hover:opacity-100 p-1 hover:bg-dark-600 rounded"
                    >
                      <Trash2 className="w-3 h-3 text-gray-500" />
                    </button>
                  </div>
                ))
              )}
            </div>
            <div className="px-4 py-2 border-t border-dark-700 text-[10px] text-gray-500">
              Click to insert • Ctrl+H to toggle
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default AgentConsole;
