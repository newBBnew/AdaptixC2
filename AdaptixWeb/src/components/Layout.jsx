import React, { useState } from 'react';
import Sidebar from './Sidebar';
import { cn } from '../utils/cn';

const Layout = ({ children, onLogout }) => {
  const [isSidebarCollapsed, setIsSidebarCollapsed] = useState(false);

  return (
    <div className={cn("flex h-screen bg-theme-glass overflow-hidden relative")}>
      {/* Dynamic Background Elements */}
      <div className="absolute inset-0 overflow-hidden pointer-events-none z-0">
        <div className="absolute top-[-10%] left-[-10%] w-[40%] h-[40%] rounded-full bg-theme-accent/5 blur-[120px] animate-pulse" />
        <div className="absolute bottom-[-10%] right-[-10%] w-[40%] h-[40%] rounded-full bg-theme-accent-secondary/5 blur-[120px] animate-pulse delay-700" />
      </div>

      <Sidebar 
        onLogout={onLogout} 
        isCollapsed={isSidebarCollapsed} 
        onToggle={() => setIsSidebarCollapsed(!isSidebarCollapsed)} 
      />
      <main className="flex-1 flex flex-col min-w-0 overflow-hidden relative z-10">
        <div className="flex-1 w-full h-full max-w-full overflow-hidden">
          {children}
        </div>
      </main>
    </div>
  );
};

export default Layout;
