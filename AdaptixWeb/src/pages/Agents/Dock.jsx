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
    { id: 'procs', label: 'Processes', icon: Activity },
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
      "flex flex-col glass-panel border-t border-theme-glass transition-all duration-300 select-none overflow-hidden",
      isDockExpanded ? "h-full" : "h-[40px]"
    )}>
      {/* Dock Header/Tabs */}
      <div className="relative flex items-center justify-center px-1 glass-card-sm border-b border-theme-glass-light h-[44px] shrink-0">
        <div className="flex items-center overflow-x-auto no-scrollbar h-full max-w-[calc(100%-50px)]">
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
                  "group flex items-center space-x-2.5 px-4 h-full text-[11px] font-semibold uppercase tracking-wider transition-all border-r border-theme-glass-light cursor-pointer relative overflow-hidden shrink-0",
                  isActive
                    ? "text-theme-primary bg-theme-glass border-b-2 border-theme-accent" 
                    : "text-theme-muted hover:text-theme-primary hover:bg-theme-hover"
                )}
              >
                {isActive && (
                  <div className="absolute top-0 left-0 right-0 h-[2px] bg-gradient-to-r from-theme-accent to-theme-accent-secondary" />
                )}

                <Icon size={14} className={cn("transition-colors", isActive ? "text-theme-accent" : "text-theme-muted")} />
                <span className="truncate max-w-[150px]">{tab.title}</span>
                
                {tab.id !== 'logs' && (
                  <button
                    onClick={(e) => {
                      e.stopPropagation();
                      closeTab(tab.id);
                    }}
                    className="p-1 rounded-lg hover:bg-theme-hover transition-colors opacity-0 group-hover:opacity-100 ml-1 hover:text-theme-danger"
                  >
                    <X size={12} strokeWidth={3} />
                  </button>
                )}
              </div>
            );
          })}
        </div>
        
        <div className="absolute right-0 top-0 bottom-0 flex items-center space-x-1 px-3 border-l border-theme-glass-light glass-card-sm z-10">
          <button 
            onClick={() => setIsDockExpanded(!isDockExpanded)}
            className="p-1.5 glass-btn text-theme-muted hover:text-theme-accent transition-all"
            title={isDockExpanded ? "COLLAPSE_VIEW" : "EXPAND_VIEW"}
          >
            {isDockExpanded ? <ChevronDown size={16} strokeWidth={2.5} /> : <ChevronUp size={16} strokeWidth={2.5} />}
          </button>
        </div>
      </div>

      {/* Dock Content */}
      {isDockExpanded && (
        <div className="flex-1 min-h-0 overflow-hidden">
          {openTabs.map(tab => (
            <div 
              key={tab.id} 
              className={cn("h-full flex flex-col", activeTabId === tab.id ? "flex" : "hidden")}
              style={{ minHeight: 0 }}
            >
              {tab.type === 'agent' ? (
                <>
                  <div className="flex items-center px-2 h-9 glass-card-sm border-b border-theme-glass-light shrink-0">
                    {agentSubTabs.map((sub) => (
                      <button
                        key={sub.id}
                        onClick={() => setActiveSubTab?.(tab.id, sub.id)}
                        className={cn(
                          "flex items-center space-x-2 px-4 h-full text-[10px] font-semibold uppercase tracking-wider transition-all relative group rounded-t-lg",
                          (tab.activeSubTab || 'console') === sub.id
                            ? "text-theme-accent bg-theme-glass border-b-2 border-theme-accent"
                            : "text-theme-muted hover:text-theme-primary hover:bg-theme-hover"
                        )}
                      >
                        <sub.icon size={12} className={cn("transition-colors", (tab.activeSubTab || 'console') === sub.id ? "text-theme-accent" : "group-hover:text-theme-secondary")} />
                        <span>{sub.label}</span>
                      </button>
                    ))}
                  </div>
                  <div className="flex-1 overflow-hidden" style={{ minHeight: 0 }}>
                    {(tab.activeSubTab || 'console') === 'console' && <AgentConsole agent={tab} />}
                    {tab.activeSubTab === 'files' && <FileBrowser agent={tab} />}
                    {tab.activeSubTab === 'procs' && <ProcessBrowser agent={tab} />}
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
