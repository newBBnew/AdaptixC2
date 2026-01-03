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

const AgentConsole = ({ agent: initialAgent }) => {
  const { agents, setActiveSubTab, consoleHistory, addConsoleLine, agentConfigs } = useAgents();
  
  // Merge live agent data (from Context) with UI state (from openTabs/props)
  // This ensures 'Info' tab shows real-time data while preserving UI state like activeSubTab
  const liveAgent = agents.find(a => a.a_id === initialAgent.a_id);
  const agent = { ...initialAgent, ...(liveAgent || {}) };

  const activeSubTab = agent.activeSubTab || 'console';
  const [inputValue, setInputValue] = useState('');
  const [commandHistory, setCommandHistory] = useState([]);
  const [historyIndex, setCommandHistoryIndex] = useState(-1);
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [searchIndex, setSearchIndex] = useState(0);
  const [isHistoryDialogOpen, setIsHistoryDialogOpen] = useState(false);
  const [isUserScrolling, setIsUserScrolling] = useState(false);
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

  // Handle scroll events to detect if user is viewing history
  const handleScroll = (e) => {
    const { scrollTop, scrollHeight, clientHeight } = e.target;
    // If user is not at the bottom (threshold 50px), consider them scrolling
    const isAtBottom = scrollHeight - scrollTop - clientHeight < 50;
    setIsUserScrolling(!isAtBottom);
  };

  // Auto-scroll to bottom only if user is not viewing history
  useEffect(() => {
    if (scrollRef.current && !isUserScrolling) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [history, isUserScrolling]);

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
        agent_id: agent.a_id,
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
    <div className="flex flex-col h-full bg-dark-900 select-none overflow-hidden">
      {/* Tab Content */}
      <div className="flex-1 overflow-hidden flex flex-col relative bg-dark-950/20">
        {activeSubTab === 'console' && (
          <div className="flex-1 flex flex-col overflow-hidden">
            {/* Search Panel (Ctrl+F) */}
            {isSearchVisible && (
              <div className="flex items-center px-3 py-1.5 bg-dark-800 border-b border-dark-700 space-x-3 animate-in slide-in-from-top-1 duration-150 z-20">
                <Search className="w-3.5 h-3.5 text-accent-primary" />
                <input
                  ref={searchInputRef}
                  type="text"
                  value={searchQuery}
                  onChange={(e) => { setSearchQuery(e.target.value); setSearchIndex(0); }}
                  placeholder="SEARCH_CONSOLE_BUFFER..."
                  className="flex-1 qt-input text-[10px]"
                  onKeyDown={(e) => {
                    if (e.key === 'Escape') setIsSearchVisible(false);
                    if (e.key === 'Enter') navigateSearch('down');
                  }}
                />
                <div className="flex items-center space-x-2">
                  <span className="text-[9px] text-gray-500 font-mono bg-dark-950 px-1.5 rounded border border-dark-700">
                    {searchMatches.length > 0 ? `${searchIndex + 1}/${searchMatches.length}` : '0/0'}
                  </span>
                  <div className="flex items-center bg-dark-900 border border-dark-700 rounded-sm">
                    <button onClick={() => navigateSearch('up')} className="p-1 hover:bg-dark-700 text-gray-500 hover:text-white transition-colors border-r border-dark-700"><ArrowUp size={12} /></button>
                    <button onClick={() => navigateSearch('down')} className="p-1 hover:bg-dark-700 text-gray-500 hover:text-white transition-colors"><ArrowDown size={12} /></button>
                  </div>
                  <button onClick={() => setIsSearchVisible(false)} className="p-1 hover:bg-dark-700 text-gray-500 hover:text-accent-danger transition-colors"><X size={14} /></button>
                </div>
              </div>
            )}

            <div 
              ref={scrollRef}
              onScroll={handleScroll}
              className="flex-1 overflow-y-auto p-4 font-mono text-[12px] space-y-0.5 scrollbar-thin select-text bg-dark-950/50 leading-relaxed"
            >
              {history.map((item, idx) => {
                if (item.type === 'clear') return null;
                const prefixInfo = getMsgPrefix(item.msgType);

                // Handle special Task type rendering
                if (item.type === 'task') {
                  return (
                    <div key={idx} className="group hover:bg-white/5 transition-colors rounded-sm px-1 -mx-1">
                       <div className="flex items-start">
                         {item.time && <span className="text-[#505050] text-[9px] mr-2 mt-0.5">[{new Date(item.time * 1000).toLocaleTimeString([], { hour12: false })}]</span>}
                         <div className="flex-1 min-w-0">
                           {item.content ? (
                              <>
                                {prefixInfo.prefix && <span className={cn("mr-1 font-black", prefixInfo.color)}>{prefixInfo.prefix}</span>}
                                <span className={cn("break-all whitespace-pre-wrap", getMsgColor(item.msgType))}>
                                  {item.content}
                                </span>
                              </>
                           ) : (
                             /* Status update logic for task without content */
                             item.completed ? (
                               <span className={cn("font-bold", item.msgType === 2 || item.msgType === 4 ? "text-[#E32227]" : "text-[#39FF14]")}>
                                 {item.msgType === 2 || item.msgType === 4 ? `[-] Task ${item.taskId} Failed` : `[+] Task ${item.taskId} Completed`}
                               </span>
                             ) : (
                               <span className="text-[#89CFF0] italic">
                                 {`[*] Task ${item.taskId} Issued: ${item.cmdline || 'Processing...'}`}
                               </span>
                             )
                           )}
                         </div>
                       </div>
                    </div>
                  );
                }

                return (
                  <div key={idx} className="group hover:bg-white/5 transition-colors rounded-sm px-1 -mx-1">
                    {item.type === 'input' ? (
                      <div className="flex items-center opacity-90">
                        {item.time && <span className="text-[#606060] text-[9px] mr-2">[{new Date(item.time * 1000).toLocaleTimeString([], { hour12: false })}]</span>}
                        <span className="text-accent-primary font-black mr-2">adaptix&gt;</span>
                        <span className="whitespace-pre-wrap font-bold text-white tracking-tight">{item.content}</span>
                      </div>
                    ) : item.type === 'info' ? (
                      <div className="flex items-center">
                        <span className="text-accent-secondary/60 italic text-[11px] font-medium border-l border-accent-secondary/20 pl-2 ml-1 my-1">{item.content}</span>
                      </div>
                    ) : (
                      <div className="flex items-start">
                        {item.time && <span className="text-[#505050] text-[9px] mr-2 mt-0.5">[{new Date(item.time * 1000).toLocaleTimeString([], { hour12: false })}]</span>}
                        <div className="flex-1 min-w-0">
                          {prefixInfo.prefix && <span className={cn("mr-1 font-black", prefixInfo.color)}>{prefixInfo.prefix}</span>}
                          <span className={cn("break-all whitespace-pre-wrap", getMsgColor(item.msgType))}>
                            {item.content}
                          </span>
                        </div>
                      </div>
                    )}
                  </div>
                );
              })}
            </div>
            
            <form onSubmit={handleSubmit} className="p-2 bg-dark-800 border-t border-dark-700 flex items-center space-x-3 shrink-0">
              <div className="flex items-center space-x-2 ml-2">
                <span className="text-accent-primary font-black text-[10px] uppercase tracking-widest">command</span>
                <ChevronRight size={14} className="text-gray-600" />
              </div>
              <input
                ref={inputRef}
                type="text"
                autoFocus
                className="flex-1 bg-dark-950 border border-dark-700 rounded-sm px-3 py-1.5 text-gray-200 font-mono text-[12px] placeholder:text-gray-700 focus:border-accent-primary/50 outline-none transition-all shadow-inner"
                placeholder="EXECUTE_CMDLINE_ENTRY..."
                value={inputValue}
                onChange={(e) => setInputValue(e.target.value)}
                onKeyDown={handleKeyDown}
              />
              <div className="flex items-center space-x-2 text-[9px] font-black text-gray-600 uppercase pr-2">
                <span className="hidden md:inline">Ctrl+H HISTORY</span>
                <div className="w-px h-3 bg-dark-700 mx-1 hidden md:block" />
                <button type="submit" className="text-gray-500 hover:text-accent-primary transition-all p-1 bg-dark-900 rounded-sm border border-dark-700">
                  <Send size={14} />
                </button>
              </div>
            </form>
          </div>
        )}

        {activeSubTab === 'files' && (
          <div className="flex-1 overflow-hidden">
            <FileBrowser agent={agent} />
          </div>
        )}
        
        {activeSubTab === 'procs' && (
          <div className="flex-1 overflow-hidden">
            <ProcessBrowser agent={agent} />
          </div>
        )}

        {activeSubTab === 'info' && (
          <div className="flex-1 p-6 overflow-auto custom-scrollbar">
            <div className="mb-8 border-l-4 border-accent-primary pl-6">
              <h3 className="text-sm font-black text-white uppercase tracking-[0.2em]">Node Telemetry Metadata</h3>
              <p className="text-[10px] text-gray-500 uppercase font-bold tracking-tight">Full fingerprint of the established session</p>
            </div>
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-3">
              {Object.entries(agent).filter(([k]) => k.startsWith('a_')).map(([key, value]) => (
                <div key={key} className="qt-panel p-3 bg-dark-900/50 hover:bg-dark-800/50 transition-all group">
                  <p className="text-[9px] uppercase text-gray-600 font-black mb-1.5 tracking-widest group-hover:text-gray-400 transition-colors">{key.replace('a_', '')}</p>
                  <p className="text-[11px] text-gray-300 font-mono truncate select-text">{String(value || 'NULL_SET')}</p>
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
