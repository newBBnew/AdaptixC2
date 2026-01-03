import React, { useState, useCallback } from 'react';
import { Mosaic, MosaicWindow, MosaicZeroState } from 'react-mosaic-component';
import { 
  Radio, 
  ListTodo, 
  Download, 
  Image as ImageIcon, 
  Key, 
  Target, 
  ScrollText,
  MessageSquare,
  Shield,
  Database,
  Terminal,
  X,
  Maximize2,
  Minimize2
} from 'lucide-react';
import { cn } from '../utils/cn';

import ListenersList from '../pages/Agents/ListenersList';
import TasksList from '../pages/Agents/TasksList';
import LogsList from '../pages/Agents/LogsList';
import ChatList from '../pages/Agents/ChatList';
import TunnelsList from '../pages/Agents/TunnelsList';
import FileDeliveryList from '../pages/Agents/FileDeliveryList';
import DownloadsList from '../pages/Agents/DownloadsList';
import TargetsList from '../pages/Agents/TargetsList';
import CredentialsList from '../pages/Agents/CredentialsList';
import ScreenshotsList from '../pages/Agents/ScreenshotsList';
import AgentConsole from '../pages/Agents/AgentConsole';

const PANEL_CONFIGS = {
  listeners: { title: 'Listeners', icon: Radio, component: ListenersList },
  logs: { title: 'Logs', icon: ScrollText, component: LogsList },
  chat: { title: 'Chat', icon: MessageSquare, component: ChatList },
  tasks: { title: 'Tasks', icon: ListTodo, component: TasksList },
  tunnels: { title: 'Tunnels', icon: Shield, component: TunnelsList },
  delivery: { title: 'File Delivery', icon: Database, component: FileDeliveryList },
  downloads: { title: 'Downloads', icon: Download, component: DownloadsList },
  targets: { title: 'Targets', icon: Target, component: TargetsList },
  creds: { title: 'Credentials', icon: Key, component: CredentialsList },
  screens: { title: 'Screenshots', icon: ImageIcon, component: ScreenshotsList },
};

const DockableLayout = ({ 
  sessionContent,
  openAgentTabs = [],
  activeAgentId,
  onAgentTabChange,
  onAgentTabClose,
  activeDock,
  setActiveDock
}) => {
  const [mosaicValue, setMosaicValue] = useState({
    direction: 'column',
    first: 'sessions',
    second: 'dock',
    splitPercentage: 60,
  });

  const renderTile = useCallback((id, path) => {
    if (id === 'sessions') {
      return (
        <MosaicWindow
          path={path}
          title="Sessions"
          toolbarControls={[]}
          className="mosaic-window-dark"
        >
          <div className="h-full w-full overflow-hidden bg-theme-glass-panel">
            {sessionContent}
          </div>
        </MosaicWindow>
      );
    }

    if (id === 'dock') {
      const config = PANEL_CONFIGS[activeDock];
      const PanelComponent = config?.component;
      const Icon = config?.icon || Radio;

      return (
        <MosaicWindow
          path={path}
          title=""
          toolbarControls={[]}
          className="mosaic-window-dark"
        >
          <div className="h-full w-full flex flex-col overflow-hidden bg-theme-glass-panel">
            {/* Dock Tab Headers */}
            <div className="flex items-center bg-theme-glass border-b border-theme-glass-light overflow-x-auto no-scrollbar shrink-0">
              {Object.entries(PANEL_CONFIGS).map(([key, cfg]) => (
                <button
                  key={key}
                  onClick={() => setActiveDock(key)}
                  className={cn(
                    "flex items-center space-x-2 px-3 py-1.5 text-[10px] font-black uppercase tracking-widest transition-all border-b-2 whitespace-nowrap",
                    activeDock === key
                      ? "text-theme-accent border-theme-accent bg-theme-accent/5"
                      : "text-theme-muted border-transparent hover:text-theme-primary hover:bg-theme-hover"
                  )}
                >
                  <cfg.icon className="w-3 h-3" />
                  <span>{cfg.title}</span>
                </button>
              ))}
              
              {/* Agent Console Tabs */}
              {openAgentTabs.map((agent) => (
                <div
                  key={agent.a_id}
                  onClick={() => onAgentTabChange?.(agent.a_id)}
                  className={cn(
                    "group flex items-center space-x-2 px-3 py-1.5 text-[10px] font-black uppercase tracking-widest transition-all border-b-2 whitespace-nowrap cursor-pointer",
                    activeAgentId === agent.a_id
                      ? "text-theme-accent-secondary border-theme-accent-secondary bg-theme-accent-secondary/5"
                      : "text-theme-muted border-transparent hover:text-theme-primary hover:bg-theme-hover"
                  )}
                >
                  <Terminal className="w-3 h-3" />
                  <span>{agent.a_name || agent.a_id.substring(0, 8)}</span>
                  <button
                    onClick={(e) => {
                      e.stopPropagation();
                      onAgentTabClose?.(agent.a_id);
                    }}
                    className="p-0.5 rounded hover:bg-theme-glass-panel transition-colors opacity-0 group-hover:opacity-100"
                  >
                    <X className="w-2.5 h-2.5" />
                  </button>
                </div>
              ))}
            </div>

            {/* Panel Content */}
            <div className="flex-1 overflow-hidden">
              {activeAgentId ? (
                openAgentTabs.map(agent => (
                  <div 
                    key={agent.a_id} 
                    className={cn("h-full", activeAgentId === agent.a_id ? "block" : "hidden")}
                  >
                    <AgentConsole agent={agent} />
                  </div>
                ))
              ) : (
                PanelComponent && <PanelComponent />
              )}
            </div>
          </div>
        </MosaicWindow>
      );
    }

    return null;
  }, [sessionContent, activeDock, setActiveDock, openAgentTabs, activeAgentId, onAgentTabChange, onAgentTabClose]);

  return (
    <div className="h-full w-full mosaic-dark-theme">
      <Mosaic
        renderTile={renderTile}
        value={mosaicValue}
        onChange={setMosaicValue}
        className="mosaic-blueprint-theme"
        zeroStateView={<MosaicZeroState createNode={() => 'sessions'} />}
      />
    </div>
  );
};

export default DockableLayout;
