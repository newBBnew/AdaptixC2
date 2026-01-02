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
  Activity
} from 'lucide-react';
import { cn } from '../../utils/cn';

const Dock = ({ 
  activeDock, 
  setActiveDock,
  openAgentTabs = [],
  activeAgentId,
  onAgentTabChange,
  onAgentTabClose
}) => {
  const [isExpanded, setIsExpanded] = useState(true);
  const { setActiveSubTab } = useAgents();

  const agentSubTabs = [
    { id: 'console', label: 'Console', icon: Terminal },
    { id: 'terminal', label: 'Terminal', icon: Terminal },
    { id: 'files', label: 'Files', icon: FolderOpen },
    { id: 'processes', label: 'Processes', icon: Activity },
  ];

  const dockItems = [
    { id: 'listeners', label: 'Listeners', icon: Radio },
    { id: 'logs', label: 'Logs', icon: ScrollText },
    { id: 'chat', label: 'Chat', icon: MessageSquare },
    { id: 'tasks', label: 'Tasks', icon: ListTodo },
    { id: 'tunnels', label: 'Tunnels', icon: Shield },
    { id: 'delivery', label: 'Delivery', icon: Database },
    { id: 'downloads', label: 'Downloads', icon: Download },
    { id: 'targets', label: 'Targets', icon: Target },
    { id: 'creds', label: 'Credentials', icon: Key },
    { id: 'screens', label: 'Screenshots', icon: ImageIcon },
  ];

  return (
    <div className={cn(
      "flex flex-col bg-dark-800 border-t border-dark-700 transition-all duration-300",
      isExpanded ? "h-1/3 min-h-[250px]" : "h-10"
    )}>
      {/* Dock Header/Tabs */}
      <div className="flex items-center justify-between px-2 bg-dark-900/50 border-b border-dark-700 h-10">
        <div className="flex items-center overflow-x-auto no-scrollbar">
          {dockItems.map((item) => (
            <button
              key={item.id}
              onClick={() => {
                setActiveDock(item.id);
                setIsExpanded(true);
              }}
              className={cn(
                "flex items-center space-x-2 px-4 py-2 text-[10px] font-black uppercase tracking-widest transition-all border-b-2 h-10 whitespace-nowrap",
                activeDock === item.id && isExpanded && !activeAgentId
                  ? "text-accent-primary border-accent-primary bg-accent-primary/5"
                  : "text-gray-500 border-transparent hover:text-gray-300"
              )}
            >
              <item.icon className="w-3.5 h-3.5" />
              <span>{item.label}</span>
            </button>
          ))}
          
          {/* Agent Console Tabs */}
          {openAgentTabs.map((agent) => (
            <div
              key={agent.a_id}
              onClick={() => {
                onAgentTabChange?.(agent.a_id);
                setIsExpanded(true);
              }}
              className={cn(
                "group flex items-center space-x-2 px-4 py-2 text-[10px] font-black uppercase tracking-widest transition-all border-b-2 h-10 whitespace-nowrap cursor-pointer",
                activeAgentId === agent.a_id && isExpanded
                  ? "text-accent-secondary border-accent-secondary bg-accent-secondary/5"
                  : "text-gray-500 border-transparent hover:text-gray-300"
              )}
            >
              <Terminal className="w-3.5 h-3.5" />
              <span>{agent.a_name || agent.a_id.substring(0, 8)}</span>
              <button
                onClick={(e) => {
                  e.stopPropagation();
                  onAgentTabClose?.(agent.a_id);
                }}
                className="p-0.5 rounded hover:bg-dark-600 transition-colors opacity-0 group-hover:opacity-100"
              >
                <X className="w-2.5 h-2.5" />
              </button>
            </div>
          ))}
        </div>
        
        <div className="flex items-center space-x-1 px-2 border-l border-dark-700 h-6">
          <button 
            onClick={() => setIsExpanded(!isExpanded)}
            className="p-1 hover:bg-dark-700 rounded text-gray-500 transition-colors"
          >
            {isExpanded ? <ChevronDown className="w-4 h-4" /> : <ChevronUp className="w-4 h-4" />}
          </button>
        </div>
      </div>

      {/* Dock Content */}
      {isExpanded && (
        <div className="flex-1 overflow-hidden bg-dark-900/30">
          {/* Agent Content with Sub-tabs */}
          {activeAgentId ? (
            openAgentTabs.map(agent => (
              <div 
                key={agent.a_id} 
                className={cn("h-full flex flex-col", activeAgentId === agent.a_id ? "block" : "hidden")}
              >
                {/* Agent Sub-tab Bar */}
                <div className="flex items-center px-2 py-1 bg-dark-800/50 border-b border-dark-700 shrink-0">
                  {agentSubTabs.map((tab) => (
                    <button
                      key={tab.id}
                      onClick={() => setActiveSubTab?.(agent.a_id, tab.id)}
                      className={cn(
                        "flex items-center space-x-1.5 px-3 py-1 text-[9px] font-bold uppercase tracking-widest transition-all rounded mr-1",
                        (agent.activeSubTab || 'console') === tab.id
                          ? "text-accent-primary bg-accent-primary/10"
                          : "text-gray-500 hover:text-gray-300 hover:bg-dark-700"
                      )}
                    >
                      <tab.icon className="w-3 h-3" />
                      <span>{tab.label}</span>
                    </button>
                  ))}
                </div>
                {/* Agent Sub-tab Content */}
                <div className="flex-1 overflow-hidden">
                  {(agent.activeSubTab || 'console') === 'console' && <AgentConsole agent={agent} />}
                  {agent.activeSubTab === 'terminal' && <RemoteTerminal agent={agent} />}
                  {agent.activeSubTab === 'files' && <FileBrowser agent={agent} />}
                  {agent.activeSubTab === 'processes' && <ProcessBrowser agent={agent} />}
                </div>
              </div>
            ))
          ) : (
            <>
              {activeDock === 'listeners' && <ListenersList />}
              {activeDock === 'tasks' && <TasksList />}
              {activeDock === 'logs' && <LogsList />}
              {activeDock === 'chat' && <ChatList />}
              {activeDock === 'tunnels' && <TunnelsList />}
              {activeDock === 'delivery' && <FileDeliveryList />}
              {activeDock === 'downloads' && <DownloadsList />}
              {activeDock === 'targets' && <TargetsList />}
              {activeDock === 'creds' && <CredentialsList />}
              {activeDock === 'screens' && <ScreenshotsList />}
            </>
          )}
        </div>
      )}
    </div>
  );
};

export default Dock;
