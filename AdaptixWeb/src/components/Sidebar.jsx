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
  ChevronRight,
  User
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
      "glass-panel flex flex-col transition-all duration-300 relative select-none border-r border-theme-glass",
      isCollapsed ? "w-14" : "w-52"
    )}>
      {/* Toggle Button */}
      <button 
        onClick={onToggle}
        className="absolute -right-3 top-16 glass-btn p-1 text-theme-muted hover:text-theme-accent z-50 transition-all shadow-glow-sm"
      >
        {isCollapsed ? <ChevronRight size={12} /> : <ChevronLeft size={12} />}
      </button>

      {/* Logo Header */}
      <div className={cn(
        "p-4 flex items-center space-x-3 border-b border-theme-glass-light overflow-hidden whitespace-nowrap",
        isCollapsed && "p-3 justify-center"
      )}>
        <div className="p-2 rounded-xl bg-theme-glass border border-theme-glass-light shrink-0 shadow-glow-sm">
          <ShieldAlert className="w-5 h-5 text-theme-accent" />
        </div>
        {!isCollapsed && (
          <div className="flex flex-col min-w-0 text-left">
            <span className="text-sm font-black uppercase tracking-[0.2em] text-theme-primary">Ops</span>
            <span className="text-[9px] font-bold text-theme-muted uppercase tracking-wider -mt-0.5">Control Suite</span>
          </div>
        )}
      </div>
      
      {/* Navigation */}
      <nav className="flex-1 p-2 space-y-1 overflow-y-auto custom-scrollbar">
        {navItems.map((item) => (
          <NavLink
            key={item.path}
            to={item.path}
            title={isCollapsed ? item.name : undefined}
            className={({ isActive }) => cn(
              "flex items-center transition-all duration-200 group overflow-hidden whitespace-nowrap rounded-xl",
              isCollapsed ? "justify-center px-0 h-11" : "px-4 py-2.5 space-x-3",
              isActive 
                ? "bg-theme-glass text-theme-primary border border-theme-glass-light shadow-glow-sm" 
                : "text-theme-muted border border-transparent hover:bg-theme-hover hover:text-theme-primary hover:border-theme-glass-light"
            )}
          >
            <item.icon className={cn(
              "w-4 h-4 transition-all shrink-0",
              "group-hover:text-theme-accent group-hover:drop-shadow-[0_0_8px_rgba(34,211,238,0.4)]"
            )} />
            {!isCollapsed && <span className="text-[11px] font-bold uppercase tracking-widest">{item.name === '控守平台' ? 'PLATFORM' : item.name}</span>}
          </NavLink>
        ))}
      </nav>

      {/* Footer */}
      <div className={cn("p-3 border-t border-theme-glass-light space-y-3", isCollapsed && "p-2")}>
        {!isCollapsed ? (
          <div className="space-y-3">
            <div className="glass-card-sm p-3 rounded-xl border border-theme-glass-light">
              <div className="flex items-center space-x-3 mb-2">
                <div className="w-8 h-8 rounded-lg bg-theme-glass flex items-center justify-center border border-theme-glass-light">
                  <User size={14} className="text-theme-accent" />
                </div>
                <div className="flex flex-col min-w-0 text-left">
                  <span className="text-[10px] font-black uppercase text-theme-primary truncate">OPERATOR</span>
                  <div className="flex items-center space-x-1">
                    <div className="w-1.5 h-1.5 rounded-full bg-theme-success animate-pulse shadow-glow-sm" />
                    <span className="text-[8px] font-bold text-theme-muted uppercase tracking-tighter">Connected</span>
                  </div>
                </div>
              </div>
              <button 
                onClick={onLogout}
                className="w-full flex items-center justify-center space-x-2 py-1.5 rounded-lg text-[9px] font-black uppercase tracking-widest text-theme-danger hover:bg-theme-danger/10 transition-all border border-transparent hover:border-theme-danger/20"
              >
                <LogOut size={12} />
                <span>Terminate</span>
              </button>
            </div>
          </div>
        ) : (
          <button 
            onClick={onLogout}
            className="w-full h-10 flex items-center justify-center rounded-xl text-theme-danger hover:bg-theme-danger/10 transition-all border border-transparent hover:border-theme-danger/20"
            title="Terminate Session"
          >
            <LogOut size={16} />
          </button>
        )}
      </div>
    </aside>
  );
};

export default Sidebar;
