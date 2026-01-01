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
  Database
} from 'lucide-react';
import { cn } from '../../utils/cn';

const Dock = ({ activeDock, setActiveDock }) => {
  const [isExpanded, setIsExpanded] = useState(true);

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
                activeDock === item.id && isExpanded
                  ? "text-accent-primary border-accent-primary bg-accent-primary/5"
                  : "text-gray-500 border-transparent hover:text-gray-300"
              )}
            >
              <item.icon className="w-3.5 h-3.5" />
              <span>{item.label}</span>
            </button>
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
        <div className="flex-1 overflow-auto bg-dark-900/30">
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
          {!['listeners', 'tasks', 'logs', 'chat', 'tunnels', 'delivery', 'downloads', 'targets', 'creds', 'screens'].includes(activeDock) && (
            <div className="flex items-center justify-center h-full text-gray-600 italic text-sm">
              {activeDock.toUpperCase()} panel content will be loaded here.
            </div>
          )}
        </div>
      )}
    </div>
  );
};

export default Dock;
