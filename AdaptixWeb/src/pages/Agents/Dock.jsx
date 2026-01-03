import React, { useState } from 'react';
import ListenersList from './ListenersList';
import TasksList from './TasksList';
import LogsList from './LogsList';
import ChatList from './ChatList';
import TunnelsList from './TunnelsList';
import FileDeliveryList from './FileDeliveryList';
import DownloadsList from './DownloadsList';
import TargetsList from './TargetsList';
import CredentialsList from './CredentialsList';
import ScreenshotsList from './ScreenshotsList';
import AgentConsole from './AgentConsole';
import RemoteTerminal from './RemoteTerminal';
import FileBrowser from './FileBrowser';
import ProcessBrowser from './ProcessBrowser';
import AxConsole from './AxConsole';
import { useAgents } from '../../context/AgentContext';
import { 
  Radio, 
  ListTodo, 
  Download, 
  Image as ImageIcon, 
  Key, 
  Target, 
  Wind,
  ScrollText,
  MessageSquare,
  ChevronDown,
  ChevronUp,
  X,
  Shield,
  Database,
  Terminal,
  FolderOpen,
  Activity,
  Code2,
  Info
} from 'lucide-react';
import { cn } from '../../utils/cn';

const Dock = () => {
  const { 
    openTabs, 
    activeTabId, 
    setActiveTabId, 
    setActiveSubTab, 
    closeTab,
    isDockExpanded,
    setIsDockExpanded 
  } = useAgents();

  const agentSubTabs = [
    { id: 'console', label: 'Console', icon: Terminal },
    { id: 'files', label: 'File Browser', icon: FolderOpen },
    { id: 'processes', label: 'Processes', icon: Activity },
    { id: 'info', label: 'Info', icon: Info },
  ];

  const getIconForType = (type) => {
    switch (type) {
      case 'listeners': return Radio;
      case 'axconsole': return Code2;
      case 'logs': return ScrollText;
      case 'chat': return MessageSquare;
      case 'tasks': return ListTodo;
      case 'tunnels': return Shield;
      case 'delivery': return Database;
      case 'downloads': return Download;
      case 'targets': return Target;
      case 'creds': return Key;
      case 'screens': return ImageIcon;
      case 'agent': return Terminal;
      default: return ScrollText;
    }
  };

  return (
    <div className={cn(
      "flex flex-col bg-dark-900 border-t border-dark-700 transition-all duration-300 select-none overflow-hidden shadow-[0_-10px_30px_rgba(0,0,0,0.5)]",
      isDockExpanded ? "h-full" : "h-[40px]"
    )}>
      {/* Dock Header/Tabs */}
      <div className="relative flex items-center justify-center px-1 bg-dark-800 border-b border-dark-700 h-[40px] shrink-0">
        <div className="flex items-center overflow-x-auto no-scrollbar h-full max-w-[calc(100%-40px)]">
          {openTabs.map((tab) => {
            const Icon = getIconForType(tab.type);
            const isActive = activeTabId === tab.id && isDockExpanded;
            return (
              <div
                key={tab.id}
                onClick={() => {
                  setActiveTabId(tab.id);
                  setIsDockExpanded(true);
                }}
                className={cn(
                  "group flex items-center space-x-2.5 px-4 h-full text-[10px] font-black uppercase tracking-widest transition-all border-r border-dark-700/50 cursor-default relative overflow-hidden shrink-0",
                  isActive
                    ? (tab.type === 'agent' 
                        ? "text-accent-secondary bg-accent-secondary/5 border-b-2 border-b-accent-secondary shadow-[inset_0_-4px_10px_rgba(16,185,129,0.05)]" 
                        : "text-accent-primary bg-accent-primary/5 border-b-2 border-b-accent-primary shadow-[inset_0_-4px_10px_rgba(61,139,106,0.05)]")
                    : "text-gray-500 hover:text-gray-300 hover:bg-dark-700/30"
                )}
              >
                {/* Active Indicator Top */}
                {isActive && (
                  <div className={cn(
                    "absolute top-0 left-0 right-0 h-[1px]",
                    tab.type === 'agent' ? "bg-accent-secondary/30" : "bg-accent-primary/30"
                  )} />
                )}

                <Icon size={14} className={cn("transition-colors", isActive ? (tab.type === 'agent' ? "text-accent-secondary" : "text-accent-primary") : "text-gray-600")} />
                <span className="truncate max-w-[150px]">{tab.title}</span>
                
                {tab.id !== 'logs' && (
                  <button
                    onClick={(e) => {
                      e.stopPropagation();
                      closeTab(tab.id);
                    }}
                    className="p-0.5 rounded hover:bg-dark-600/50 transition-colors opacity-0 group-hover:opacity-100 ml-1 hover:text-accent-danger"
                  >
                    <X size={12} strokeWidth={3} />
                  </button>
                )}
              </div>
            );
          })}
        </div>
        
        <div className="absolute right-0 top-0 bottom-0 flex items-center space-x-1 px-3 border-l border-dark-700 bg-dark-800 z-10">
          <button 
            onClick={() => setIsDockExpanded(!isDockExpanded)}
            className="p-1 hover:bg-dark-700 rounded text-gray-500 hover:text-white transition-all border border-transparent hover:border-dark-600"
            title={isDockExpanded ? "COLLAPSE_VIEW" : "EXPAND_VIEW"}
          >
            {isDockExpanded ? <ChevronDown size={16} strokeWidth={2.5} /> : <ChevronUp size={16} strokeWidth={2.5} />}
          </button>
        </div>
      </div>

      {/* Dock Content */}
      {isDockExpanded && (
        <div className="flex-1 overflow-hidden bg-dark-950/20">
          {openTabs.map(tab => (
            <div 
              key={tab.id} 
              className={cn("h-full flex flex-col", activeTabId === tab.id ? "block" : "hidden")}
            >
              {tab.type === 'agent' ? (
                <>
                  <div className="flex items-center px-2 h-8 bg-dark-800/80 border-b border-dark-700 shrink-0 shadow-sm">
                    {agentSubTabs.map((sub) => (
                      <button
                        key={sub.id}
                        onClick={() => setActiveSubTab?.(tab.id, sub.id)}
                        className={cn(
                          "flex items-center space-x-2 px-4 h-full text-[9px] font-black uppercase tracking-widest transition-all relative group",
                          (tab.activeSubTab || 'console') === sub.id
                            ? "text-accent-primary border-b-2 border-accent-primary"
                            : "text-gray-600 hover:text-gray-300"
                        )}
                      >
                        <sub.icon size={12} className={cn("transition-colors", (tab.activeSubTab || 'console') === sub.id ? "text-accent-primary" : "group-hover:text-gray-400")} />
                        <span>{sub.label}</span>
                      </button>
                    ))}
                  </div>
                  <div className="flex-1 overflow-hidden">
                    {(tab.activeSubTab || 'console') === 'console' && <AgentConsole agent={tab} />}
                    {tab.activeSubTab === 'files' && <FileBrowser agent={tab} />}
                    {tab.activeSubTab === 'processes' && <ProcessBrowser agent={tab} />}
                    {tab.activeSubTab === 'info' && <AgentConsole agent={{...tab, activeSubTab: 'info'}} />}
                  </div>
                </>
              ) : (
                <div className="h-full overflow-hidden">
                  {tab.type === 'listeners' && <ListenersList />}
                  {tab.type === 'axconsole' && <AxConsole />}
                  {tab.type === 'tasks' && <TasksList />}
                  {tab.type === 'logs' && <LogsList />}
                  {tab.type === 'chat' && <ChatList />}
                  {tab.type === 'tunnels' && <TunnelsList />}
                  {tab.type === 'delivery' && <FileDeliveryList />}
                  {tab.type === 'downloads' && <DownloadsList />}
                  {tab.type === 'targets' && <TargetsList />}
                  {tab.type === 'creds' && <CredentialsList />}
                  {tab.type === 'screens' && <ScreenshotsList />}
                </div>
              )}
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

export default Dock;
