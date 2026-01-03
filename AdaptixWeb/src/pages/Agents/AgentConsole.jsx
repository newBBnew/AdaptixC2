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
  const { agents, setActiveSubTab, consoleHistory, addConsoleLine, agentConfigs, processCommand } = useAgents();
  
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
      case 1: return { prefix: '[+] ', color: 'text-theme-success' };
      case 2: return { prefix: '[-] ', color: 'text-theme-danger' };
      case 5: return { prefix: '[*] ', color: 'text-theme-accent' };
      default: return { prefix: '', color: 'text-theme-muted' };
    }
  };

  const getMsgColor = (msgType) => {
    switch (msgType) {
      case 1: return 'text-theme-success font-black';
      case 2: return 'text-theme-danger font-black';
      case 3: return 'text-theme-warning font-bold';
      case 4: return 'text-theme-danger font-black uppercase tracking-widest';
      case 5: return 'text-theme-accent italic';
      case 6: return 'text-theme-primary opacity-80';
      case 10: return 'text-theme-primary';
      case 11: return 'text-theme-muted';
      default: return 'text-theme-secondary';
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
    addConsoleLine(agent.a_id, { type: 'input', content: cmd, time: Math.floor(Date.now() / 1000) });

    try {
      // 1. First, check if processCommand exists in context (Extension-Kit logic)
      if (typeof processCommand === 'function') {
         await processCommand(agent.a_id, cmd);
      } 
      // 2. If for some reason context is not ready, fallback to direct API
      else {
        await agentApi.executeCommand({
          name: agent.a_name,
          id: agent.a_id,
          ui: true,
          cmdline: cmd,
          data: "{}",
          ax_hook_id: "",
          ax_handler_id: ""
        });
      }
    } catch (err) {
      console.error('[AgentConsole] Command execution failed:', err);
      addConsoleLine(agent.a_id, { 
        type: 'output', 
        content: `[-] Error: ${err.response?.data?.message || err.message || 'Command failed to send'}`,
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
    <div className="flex flex-col h-full select-none overflow-hidden" style={{ minHeight: 0 }}>
      {/* Tab Content */}
      <div className="flex-1 flex flex-col relative glass-panel" style={{ minHeight: 0, overflow: 'hidden' }}>
        {activeSubTab === 'console' && (
          <div className="flex-1 flex flex-col" style={{ minHeight: 0, overflow: 'hidden' }}>
            {/* Search Panel (Ctrl+F) */}
            {isSearchVisible && (
              <div className="flex items-center px-4 py-2 glass-card-sm border-b border-theme-glass-light space-x-3 z-20">
                <Search className="w-4 h-4 text-theme-accent" />
                <input
                  ref={searchInputRef}
                  type="text"
                  value={searchQuery}
                  onChange={(e) => { setSearchQuery(e.target.value); setSearchIndex(0); }}
                  placeholder="Search console..."
                  className="flex-1 glass-input text-sm px-3 py-1.5"
                  onKeyDown={(e) => {
                    if (e.key === 'Escape') setIsSearchVisible(false);
                    if (e.key === 'Enter') navigateSearch('down');
                  }}
                />
                <div className="flex items-center space-x-2">
                  <span className="text-xs text-theme-muted font-mono glass-card-sm px-2 py-1 rounded-lg">
                    {searchMatches.length > 0 ? `${searchIndex + 1}/${searchMatches.length}` : '0/0'}
                  </span>
                  <div className="flex items-center glass-btn rounded-lg overflow-hidden">
                    <button onClick={() => navigateSearch('up')} className="p-1.5 hover:bg-theme-hover text-theme-muted hover:text-theme-accent transition-colors border-r border-theme-glass-light"><ArrowUp size={14} /></button>
                    <button onClick={() => navigateSearch('down')} className="p-1.5 hover:bg-theme-hover text-theme-muted hover:text-theme-accent transition-colors"><ArrowDown size={14} /></button>
                  </div>
                  <button onClick={() => setIsSearchVisible(false)} className="p-1.5 glass-btn text-theme-muted hover:text-theme-danger transition-colors"><X size={16} /></button>
                </div>
              </div>
            )}

            <div 
              ref={scrollRef}
              onScroll={handleScroll}
              className="flex-1 p-4 font-mono text-[12px] space-y-1 select-text glass-card-sm leading-relaxed text-theme-primary custom-scrollbar"
              style={{ minHeight: 0, maxHeight: '100%', overflowY: 'auto', overflowX: 'hidden' }}
            >
              {history.map((item, idx) => {
                if (item.type === 'clear') return null;
                const prefixInfo = getMsgPrefix(item.msgType);

                // Handle special Task type rendering
                if (item.type === 'task') {
                  return (
                    <div key={idx} className="group hover:bg-theme-hover transition-colors rounded-sm px-1 -mx-1">
                       <div className="flex items-start">
                         {item.time && <span className="text-theme-muted text-[9px] mr-2 mt-0.5 font-bold">[{new Date(item.time * 1000).toLocaleTimeString([], { hour12: false })}]</span>}
                         <div className="flex-1 min-w-0">
                           {item.content ? (
                              <>
                                {prefixInfo.prefix && <span className={cn("mr-1 font-black", prefixInfo.color)}>{prefixInfo.prefix}</span>}
                                <span className={cn("break-all whitespace-pre-wrap font-medium", getMsgColor(item.msgType))}>
                                  {item.content}
                                </span>
                              </>
                           ) : (
                             /* Status update logic for task without content */
                             item.completed ? (
                               <span className={cn("font-bold", item.msgType === 2 || item.msgType === 4 ? "text-theme-danger" : "text-theme-success")}>
                                 {item.msgType === 2 || item.msgType === 4 ? `[-] Task ${item.taskId} Failed` : `[+] Task ${item.taskId} Completed`}
                               </span>
                             ) : (
                               <span className="text-theme-accent-secondary italic font-bold">
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
                  <div key={idx} className="group hover:bg-theme-hover transition-colors rounded-sm px-1 -mx-1">
                    {item.type === 'input' ? (
                      <div className="flex items-center opacity-90">
                        {item.time && <span className="text-theme-muted text-[9px] mr-2 font-bold">[{new Date(item.time * 1000).toLocaleTimeString([], { hour12: false })}]</span>}
                        <span className="text-theme-accent font-black mr-2">adaptix&gt;</span>
                        <span className="whitespace-pre-wrap font-bold text-theme-primary tracking-tight">{item.content}</span>
                      </div>
                    ) : item.type === 'info' ? (
                      <div className="flex items-center">
                        <span className="text-theme-accent-secondary/60 italic text-[11px] font-medium border-l border-theme-glass-light pl-2 ml-1 my-1">{item.content}</span>
                      </div>
                    ) : (
                      <div className="flex items-start">
                        {item.time && <span className="text-theme-muted text-[9px] mr-2 mt-0.5 font-bold">[{new Date(item.time * 1000).toLocaleTimeString([], { hour12: false })}]</span>}
                        <div className="flex-1 min-w-0">
                          {prefixInfo.prefix && <span className={cn("mr-1 font-black", prefixInfo.color)}>{prefixInfo.prefix}</span>}
                          <span className={cn("break-all whitespace-pre-wrap font-medium", getMsgColor(item.msgType))}>
                            {item.content}
                          </span>
                        </div>
                      </div>
                    )}
                  </div>
                );
              })}
            </div>
            
            <form onSubmit={handleSubmit} className="shrink-0 p-3 glass-card-sm border-t border-theme-glass-light flex items-center space-x-3">
              <div className="flex items-center space-x-2 ml-2">
                <span className="text-theme-accent font-bold text-[11px] uppercase tracking-wider">command</span>
                <ChevronRight size={14} className="text-theme-muted" />
              </div>
              <input
                ref={inputRef}
                type="text"
                autoFocus
                className="flex-1 glass-input px-4 py-2 text-theme-primary font-mono text-[13px] placeholder:text-theme-muted"
                placeholder="Enter command..."
                value={inputValue}
                onChange={(e) => setInputValue(e.target.value)}
                onKeyDown={handleKeyDown}
              />
              <div className="flex items-center space-x-2 text-[10px] font-medium text-theme-muted uppercase pr-2">
                <span className="hidden md:inline">Ctrl+H History</span>
                <div className="w-px h-4 bg-theme-glass-light mx-1 hidden md:block" />
                <button type="submit" className="glass-btn p-2 text-theme-muted hover:text-theme-accent transition-all">
                  <Send size={16} />
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
            <div className="mb-8">
              <h3 className="text-lg font-bold gradient-text uppercase tracking-wider">Session Metadata</h3>
              <p className="text-sm text-theme-muted mt-1">Full fingerprint of the established session</p>
            </div>
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
              {Object.entries(agent).filter(([k]) => k.startsWith('a_')).map(([key, value]) => (
                <div key={key} className="glass-card-sm p-4 hover:bg-theme-hover transition-all group">
                  <p className="text-[10px] uppercase text-theme-muted font-semibold mb-2 tracking-wider group-hover:text-theme-accent transition-colors">{key.replace('a_', '')}</p>
                  <p className="text-sm text-theme-primary font-mono truncate select-text">{String(value || 'N/A')}</p>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>

      {/* Command History Dialog (Ctrl+H) */}
      {isHistoryDialogOpen && (
        <div className="fixed inset-0 z-50 bg-theme-glass-panel/40 backdrop-blur-sm flex items-center justify-center" onClick={() => setIsHistoryDialogOpen(false)}>
          <div className="glass-card w-96 max-h-96 overflow-hidden" onClick={e => e.stopPropagation()}>
            <div className="flex items-center justify-between px-5 py-4 border-b border-theme-glass-light">
              <div className="flex items-center space-x-3">
                <History className="w-5 h-5 text-theme-accent" />
                <span className="text-base font-bold text-theme-primary">Command History</span>
              </div>
              <button onClick={() => setIsHistoryDialogOpen(false)} className="p-1.5 glass-btn hover:text-theme-danger rounded-lg">
                <X className="w-4 h-4 text-theme-muted" />
              </button>
            </div>
            <div className="max-h-64 overflow-y-auto custom-scrollbar">
              {commandHistory.length === 0 ? (
                <p className="p-6 text-center text-theme-muted text-sm">No commands in history</p>
              ) : (
                commandHistory.map((cmd, idx) => (
                  <div 
                    key={idx}
                    className="px-5 py-3 hover:bg-theme-hover cursor-pointer border-b border-theme-glass-light flex items-center justify-between group"
                    onClick={() => { setInputValue(cmd); setIsHistoryDialogOpen(false); inputRef.current?.focus(); }}
                  >
                    <span className="text-sm font-mono text-theme-primary truncate">{cmd}</span>
                    <button 
                      onClick={(e) => { e.stopPropagation(); setCommandHistory(prev => prev.filter((_, i) => i !== idx)); }}
                      className="opacity-0 group-hover:opacity-100 p-1.5 glass-btn hover:text-theme-danger rounded-lg"
                    >
                      <Trash2 className="w-3.5 h-3.5 text-theme-muted" />
                    </button>
                  </div>
                ))
              )}
            </div>
            <div className="px-5 py-3 border-t border-theme-glass-light text-xs text-theme-muted">
              Click to insert • Ctrl+H to toggle
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default AgentConsole;
