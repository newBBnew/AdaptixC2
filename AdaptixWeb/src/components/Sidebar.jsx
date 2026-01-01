import React from 'react';
import { NavLink } from 'react-router-dom';
import { 
  LayoutDashboard, 
  Users, 
  Crosshair, 
  Settings, 
  ShieldAlert,
  LogOut,
  ChevronLeft,
  ChevronRight
} from 'lucide-react';
import { clsx } from 'clsx';
import { twMerge } from 'tailwind-merge';
import { cn } from '../utils/cn';

const Sidebar = ({ onLogout, isCollapsed, onToggle }) => {
  const navItems = [
    { name: 'Dashboard', path: '/dashboard', icon: LayoutDashboard },
    { name: '控守平台', path: '/control', icon: Users },
    { name: 'Tactical', path: '/tactical', icon: Crosshair },
    { name: 'Settings', path: '/settings', icon: Settings },
  ];

  return (
    <aside className={cn(
      "bg-dark-800 border-r border-dark-700 flex flex-col transition-all duration-300 relative",
      isCollapsed ? "w-16" : "w-64"
    )}>
      {/* Toggle Button */}
      <button 
        onClick={onToggle}
        className="absolute -right-3 top-20 bg-dark-700 border border-dark-600 rounded-full p-1 text-gray-400 hover:text-white z-50 hover:bg-dark-600 transition-colors"
      >
        {isCollapsed ? <ChevronRight size={14} /> : <ChevronLeft size={14} />}
      </button>

      <div className={cn(
        "p-6 flex items-center space-x-3 border-b border-dark-700 overflow-hidden whitespace-nowrap",
        isCollapsed && "p-4 justify-center"
      )}>
        <ShieldAlert className="w-8 h-8 text-accent-primary shrink-0" />
        {!isCollapsed && <span className="text-xl font-bold tracking-tight text-white">Adaptix C2</span>}
      </div>
      
      <nav className="flex-1 p-4 space-y-2 overflow-y-auto">
        {navItems.map((item) => (
          <NavLink
            key={item.path}
            to={item.path}
            title={isCollapsed ? item.name : undefined}
            className={({ isActive }) => cn(
              "flex items-center px-4 py-3 rounded-lg transition-all duration-200 group overflow-hidden whitespace-nowrap",
              isCollapsed ? "justify-center px-0" : "space-x-3",
              isActive 
                ? "bg-accent-primary/10 text-accent-primary border-l-4 border-accent-primary pl-3" 
                : "text-gray-400 hover:bg-dark-700 hover:text-gray-200"
            )}
          >
            <item.icon className={cn(
              "w-5 h-5 transition-colors shrink-0",
              "group-hover:text-accent-primary"
            )} />
            {!isCollapsed && <span className="font-medium">{item.name}</span>}
          </NavLink>
        ))}
      </nav>

      <div className={cn("p-4 border-t border-dark-700 space-y-3", isCollapsed && "p-2")}>
        {!isCollapsed ? (
          <div className="bg-dark-700/50 rounded-lg p-3 flex items-center space-x-3 overflow-hidden whitespace-nowrap">
            <div className="w-8 h-8 rounded-full bg-accent-secondary/20 flex items-center justify-center shrink-0">
              <div className="w-2 h-2 rounded-full bg-accent-secondary animate-pulse" />
            </div>
            <div className="flex-1 min-w-0">
              <p className="text-xs font-semibold text-gray-300 truncate">Teamserver Online</p>
              <p className="text-[10px] text-gray-500 truncate">{localStorage.getItem('adaptix_url') || '127.0.0.1:443'}</p>
            </div>
          </div>
        ) : (
          <div className="flex justify-center">
            <div className="w-2 h-2 rounded-full bg-accent-secondary animate-pulse" title="Online" />
          </div>
        )}
        
        <button 
          onClick={onLogout}
          title={isCollapsed ? "Disconnect" : undefined}
          className={cn(
            "w-full flex items-center rounded-lg text-gray-500 hover:bg-accent-danger/10 hover:text-accent-danger transition-all duration-200 overflow-hidden whitespace-nowrap",
            isCollapsed ? "justify-center py-3" : "px-4 py-2 space-x-3"
          )}
        >
          <LogOut className="w-4 h-4 shrink-0" />
          {!isCollapsed && <span className="text-xs font-bold uppercase tracking-wider">Disconnect</span>}
        </button>
      </div>
    </aside>
  );
};

export default Sidebar;
