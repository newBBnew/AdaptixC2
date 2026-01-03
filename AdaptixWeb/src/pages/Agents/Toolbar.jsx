import React from 'react';
import { 
  Radio, 
  ScrollText, 
  MessageSquare, 
  LayoutList, 
  Network, 
  Briefcase, 
  Shield, 
  Database, 
  Download, 
  Target, 
  Key, 
  Monitor, 
  Keyboard, 
  Link2,
  Menu,
  ChevronDown,
  Search,
  Settings as SettingsIcon
} from 'lucide-react';
import { cn } from '../../utils/cn';

const Toolbar = ({ onButtonClick, activeTabId }) => {
  const sections = [
    [
      { id: 'listeners', label: 'Listeners', icon: Radio, tooltip: 'Listeners & Sites' },
      { id: 'logs', label: 'Logs', icon: ScrollText, tooltip: 'Logs' },
      { id: 'chat', label: 'Chat', icon: MessageSquare, tooltip: 'Chat' },
    ],
    [
      { id: 'sessions', label: 'Sessions', icon: LayoutList, tooltip: 'Session table' },
      { id: 'graph', label: 'Graph', icon: Network, tooltip: 'Session graph' },
      { id: 'tasks', label: 'Tasks', icon: Briefcase, tooltip: 'Jobs & Tasks' },
    ],
    [
      { id: 'tunnels', label: 'Tunnels', icon: Shield, tooltip: 'Tunnels table' },
      { id: 'delivery', label: 'Delivery', icon: Database, tooltip: 'File Delivery' },
    ],
    [
      { id: 'downloads', label: 'Downloads', icon: Download, tooltip: 'Downloads' },
      { id: 'targets', label: 'Targets', icon: Target, tooltip: 'Targets table' },
      { id: 'creds', label: 'Credentials', icon: Key, tooltip: 'Credentials' },
      { id: 'screens', label: 'Screens', icon: Monitor, tooltip: 'Screens' },
      { id: 'keys', label: 'Keystrokes', icon: Keyboard, tooltip: 'Keystrokes', hidden: true },
    ],
    [
      { id: 'reconnect', label: 'Reconnect', icon: Link2, tooltip: 'Reconnect to C2', color: 'text-theme-success' },
    ]
  ];

  return (
    <div className="flex flex-col glass-panel border-b border-theme-glass select-none">
      {/* 1. Main Menu Bar */}
      <div className="flex items-center px-1 py-1 glass-card-sm border-b border-theme-glass-light text-[11px] font-semibold uppercase tracking-wider text-theme-muted">
        <div className="flex items-center space-x-1 px-2">
          {['Projects', 'AxScript', 'Settings', 'View'].map(menu => (
            <div key={menu} className="hover:bg-theme-hover hover:text-theme-primary cursor-pointer px-3 py-1.5 rounded-lg transition-all duration-150">
              {menu}
            </div>
          ))}
          <div className="w-px h-4 bg-theme-glass mx-2" />
          <div className="text-theme-accent hover:opacity-80 cursor-pointer px-3 py-1.5 rounded-lg transition-all font-bold">
            HELP
          </div>
        </div>
      </div>

      {/* 2. ToolBar */}
      <div className="flex items-center justify-between px-3 h-12 glass-card-sm">
        <div className="flex items-center h-full">
          {sections.map((section, idx) => (
            <React.Fragment key={idx}>
              <div className="flex items-center space-x-1 px-1">
                {section.filter(btn => !btn.hidden).map((btn) => {
                  const isActive = activeTabId === btn.id;
                  return (
                    <button
                      key={btn.id}
                      title={btn.tooltip}
                      onClick={() => onButtonClick?.(btn.id)}
                      className={cn(
                        "p-2 rounded-lg transition-all group relative flex items-center justify-center",
                        isActive 
                          ? "bg-theme-glass text-theme-accent shadow-sm" 
                          : "text-theme-muted hover:bg-theme-hover hover:text-theme-primary"
                      )}
                    >
                      <btn.icon size={18} strokeWidth={isActive ? 2.5 : 2} className={cn(
                        "transition-all duration-200",
                        isActive ? "text-theme-accent" : "group-hover:text-theme-accent"
                      )} />
                      
                      {isActive && (
                        <div className="absolute -bottom-1 left-1/2 -translate-x-1/2 w-1.5 h-1.5 bg-theme-accent rounded-full shadow-glow-sm" />
                      )}
                    </button>
                  );
                })}
              </div>
              {idx < sections.length - 1 && (
                <div className="w-px h-5 bg-theme-glass mx-2" />
              )}
            </React.Fragment>
          ))}
        </div>

        <div className="flex items-center space-x-4 pr-2">
          <div className="relative group flex items-center glass-input rounded-xl overflow-hidden transition-all">
            <Search size={14} className="ml-3 text-theme-muted group-focus-within:text-theme-accent transition-colors" />
            <input 
              type="text" 
              placeholder="Search..." 
              className="bg-transparent border-none py-2 pl-2 pr-4 text-sm text-theme-primary outline-none w-48 transition-all placeholder:text-theme-muted"
            />
          </div>
          
          <div className="flex items-center space-x-2">
            <button className="p-2 glass-btn text-theme-muted hover:text-theme-accent transition-all" title="Settings">
              <SettingsIcon size={16} strokeWidth={2} />
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};

export default Toolbar;
