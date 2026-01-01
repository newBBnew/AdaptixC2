import React, { useState } from 'react';
import Sidebar from './Sidebar';
import { cn } from '../utils/cn';

const Layout = ({ children, onLogout }) => {
  const [isSidebarCollapsed, setIsSidebarCollapsed] = useState(false);

  return (
    <div className={cn("flex h-screen bg-dark-900 overflow-hidden")}>
      <Sidebar 
        onLogout={onLogout} 
        isCollapsed={isSidebarCollapsed} 
        onToggle={() => setIsSidebarCollapsed(!isSidebarCollapsed)} 
      />
      <main className="flex-1 flex flex-col min-w-0 overflow-hidden relative">
        {children}
      </main>
    </div>
  );
};

export default Layout;
