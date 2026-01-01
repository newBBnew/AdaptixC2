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

const Toolbar = () => {
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
      { id: 'tunnel', label: 'Tunnels', icon: Shield, tooltip: 'Tunnels table' },
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
      { id: 'reconnect', label: 'Reconnect', icon: Link2, tooltip: 'Reconnect to C2', color: 'text-neon-green' },
    ]
  ];

  return (
    <div className="flex flex-col bg-dark-800 border-b border-dark-700 select-none">
      {/* 1. Main Menu Bar (Mimics QMenuBar) */}
      <div className="flex items-center px-2 py-0.5 bg-dark-900/50 border-b border-dark-700 text-[11px] text-gray-400">
        <div className="flex items-center space-x-4 px-2">
          <div className="hover:text-white cursor-default px-1 py-0.5 rounded hover:bg-dark-700 transition-colors flex items-center space-x-1">
            <span>Projects</span>
          </div>
          <div className="hover:text-white cursor-default px-1 py-0.5 rounded hover:bg-dark-700 transition-colors flex items-center space-x-1">
            <span>AxScript</span>
          </div>
          <div className="hover:text-white cursor-default px-1 py-0.5 rounded hover:bg-dark-700 transition-colors flex items-center space-x-1">
            <span>Settings</span>
          </div>
          <div className="hover:text-white cursor-default px-1 py-0.5 rounded hover:bg-dark-700 transition-colors flex items-center space-x-1 text-accent-primary/80 font-bold">
            <span>Help</span>
          </div>
        </div>
      </div>

      {/* 2. ToolBar (Mimics AdaptixWidget.cpp layout) */}
      <div className="flex items-center justify-between px-2 py-1.5 h-10">
        <div className="flex items-center">
          {sections.map((section, idx) => (
            <React.Fragment key={idx}>
              <div className="flex items-center space-x-0.5">
                {section.filter(btn => !btn.hidden).map((btn) => (
                  <button
                    key={btn.id}
                    title={btn.tooltip}
                    className={cn(
                      "p-1.5 rounded hover:bg-dark-700 transition-all group relative",
                      btn.color || "text-gray-400 hover:text-white"
                    )}
                  >
                    <btn.icon className={cn(
                      "w-4.5 h-4.5 group-active:scale-90 transition-transform",
                      !btn.color && "text-accent-primary/90"
                    )} />
                  </button>
                ))}
              </div>
              {idx < sections.length - 1 && (
                <div className="w-px h-5 bg-dark-600 mx-1.5" />
              )}
            </React.Fragment>
          ))}
        </div>

        <div className="flex items-center space-x-3 pr-2">
          <div className="relative group">
            <Search className="w-3.5 h-3.5 absolute left-2.5 top-1/2 -translate-y-1/2 text-gray-500 group-focus-within:text-accent-primary transition-colors" />
            <input 
              type="text" 
              placeholder="Search session..." 
              className="bg-dark-950/50 border border-dark-600 rounded py-1 pl-8 pr-3 text-[11px] text-gray-300 outline-none focus:ring-1 focus:ring-accent-primary/50 w-40 transition-all placeholder:text-gray-600"
            />
          </div>
          <button className="p-1.5 rounded hover:bg-dark-700 text-gray-400 hover:text-white transition-all">
            <SettingsIcon className="w-4 h-4" />
          </button>
        </div>
      </div>
    </div>
  );
};

export default Toolbar;
