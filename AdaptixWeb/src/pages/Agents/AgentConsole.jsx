import React, { useState } from 'react';
import { useAgents } from '../../context/AgentContext';
import { 
  Terminal, 
  Files, 
  Activity, 
  Info,
  ChevronRight,
  Send
} from 'lucide-react';
import { clsx } from 'clsx';
import { twMerge } from 'tailwind-merge';
import { cn } from '../../utils/cn';

const AgentConsole = ({ agent }) => {
  const { setActiveSubTab } = useAgents();
  const activeSubTab = agent.activeSubTab || 'console';
  const [inputValue, setInputValue] = useState('');
  const [history, setHistory] = useState([
    { type: 'info', content: `Session established with ${agent.a_name || agent.a_id}` },
    { type: 'info', content: 'Type "help" for a list of available commands.' }
  ]);
  const scrollRef = React.useRef(null);

  React.useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [history]);

  const handleSubmit = (e) => {
    e.preventDefault();
    if (!inputValue.trim()) return;
    
    setHistory([...history, { type: 'input', content: inputValue }]);
    // 模拟响应 - 真实下发逻辑将在下一步对接
    setTimeout(() => {
      setHistory(prev => [...prev, { type: 'output', content: `[+] Task queued: ${inputValue}` }]);
    }, 100);
    setInputValue('');
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
            <div 
              ref={scrollRef}
              className="flex-1 overflow-y-auto p-4 font-mono text-[12px] space-y-1 scrollbar-thin"
            >
              {history.map((item, idx) => (
                <div key={idx} className={cn(
                  item.type === 'input' ? "text-accent-secondary flex items-start space-x-2" : 
                  item.type === 'info' ? "text-blue-500/80 italic text-[11px]" : "text-gray-300"
                )}>
                  {item.type === 'input' && <ChevronRight className="w-3 h-3 mt-1 flex-shrink-0" />}
                  <span className="whitespace-pre-wrap">{item.content}</span>
                </div>
              ))}
            </div>
            
            <form onSubmit={handleSubmit} className="p-2 bg-dark-800/50 border-t border-dark-700 flex items-center space-x-2">
              <span className="text-accent-primary font-mono font-black text-xs ml-2">adaptix&gt;</span>
              <input
                type="text"
                autoFocus
                className="flex-1 bg-transparent outline-none text-gray-200 font-mono text-[12px]"
                placeholder="Type command..."
                value={inputValue}
                onChange={(e) => setInputValue(e.target.value)}
              />
              <button type="submit" className="text-gray-600 hover:text-accent-primary transition-colors pr-2">
                <Send className="w-3.5 h-3.5" />
              </button>
            </form>
          </>
        )}

        {activeSubTab === 'files' && (
          <div className="flex-1 flex flex-col items-center justify-center text-gray-600">
            <Files className="w-12 h-12 mb-4 opacity-20" />
            <p className="italic text-xs font-bold uppercase tracking-widest">File Browser: {agent.a_computer}</p>
            <p className="text-[10px] mt-2 opacity-50">Fetching remote filesystem...</p>
          </div>
        )}
        
        {activeSubTab === 'procs' && (
          <div className="flex-1 flex flex-col items-center justify-center text-gray-600">
            <Activity className="w-12 h-12 mb-4 opacity-20" />
            <p className="italic text-xs font-bold uppercase tracking-widest">Process Monitor: PID {agent.a_pid}</p>
            <p className="text-[10px] mt-2 opacity-50">Retrieving task list...</p>
          </div>
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
    </div>
  );
};

export default AgentConsole;
