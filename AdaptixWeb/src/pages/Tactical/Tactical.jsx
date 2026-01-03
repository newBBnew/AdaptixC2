import React from 'react';
import { Crosshair, Box, Layout, Shield, Zap, Terminal, Search, Plus } from 'lucide-react';
import { cn } from '../../utils/cn';

const Tactical = () => {
  return (
    <div className="flex flex-col h-full w-full select-none overflow-hidden">
      {/* Header */}
      <header className="px-6 py-4 glass-card-sm border-b border-theme-glass-light shrink-0">
        <div className="flex items-center space-x-4">
          <div className="p-2.5 bg-theme-glass rounded-xl border border-theme-glass-light text-theme-accent shadow-glow-sm">
            <Crosshair size={24} />
          </div>
          <div>
            <h1 className="text-lg font-black uppercase tracking-[0.2em] text-theme-primary">Tactical Orchestration</h1>
            <p className="text-xs text-theme-muted uppercase font-bold tracking-widest">Unified attack surface management & framework integration</p>
          </div>
        </div>
      </header>
      
      <div className="flex-1 p-6 grid grid-cols-1 lg:grid-cols-4 gap-6 overflow-hidden">
        {/* Main Canvas Area */}
        <div className="lg:col-span-3 glass-panel flex flex-col overflow-hidden rounded-2xl border border-theme-glass-light">
          <div className="glass-card-sm px-4 py-3 flex items-center justify-between border-b border-theme-glass-light">
            <div className="flex items-center space-x-2">
              <Layout size={16} className="text-theme-accent" />
              <span className="text-sm font-bold text-theme-primary uppercase tracking-wider">Attack Workflow Canvas</span>
            </div>
            <div className="flex items-center space-x-3">
              <span className="glass-btn px-2 py-0.5 text-[10px] text-theme-accent-secondary">IDLE</span>
              <button className="glass-btn px-3 py-1.5 flex items-center space-x-2 border border-theme-glass-light text-theme-accent hover:bg-theme-hover">
                <Plus size={14} />
                <span className="text-xs font-bold uppercase tracking-wider">New Workflow</span>
              </button>
            </div>
          </div>
          
          <div className="flex-1 flex flex-col items-center justify-center p-12 text-center relative border-t border-theme-glass-light">
            {/* Background Grid Pattern */}
            <div className="absolute inset-0 opacity-[0.05] pointer-events-none" 
                 style={{ backgroundImage: 'radial-gradient(currentColor 1px, transparent 1px)', backgroundSize: '24px 24px' }} />
            
            <div className="w-24 h-20 bg-theme-glass rounded-2xl border border-theme-glass-light flex items-center justify-center mb-8 shadow-glow-sm relative z-10 group hover:border-theme-accent transition-all duration-300">
              <Box size={48} className="text-theme-muted group-hover:text-theme-accent transition-colors" />
            </div>
            <h2 className="text-lg font-black uppercase text-theme-primary mb-3 tracking-[0.2em] relative z-10">Tactical Canvas Ready</h2>
            <p className="text-sm text-theme-muted max-w-sm uppercase font-bold tracking-widest relative z-10 leading-relaxed">
              Orchestrate MSF modules and Adaptix capabilities into a unified execution flow. 
              Drag nodes from the library to begin orchestration.
            </p>
            <button className="mt-10 glass-btn-primary px-10 py-3 rounded-xl font-black uppercase tracking-[0.2em] text-sm relative z-10 transition-all active:scale-95">
              Open Framework Browser
            </button>
          </div>
        </div>

        {/* Right Sidebar: Asset Library */}
        <div className="glass-panel flex flex-col overflow-hidden rounded-2xl border border-theme-glass-light">
          <div className="glass-card-sm px-4 py-3 flex items-center border-b border-theme-glass-light">
            <div className="flex items-center space-x-2">
              <Shield size={16} className="text-theme-danger" />
              <span className="text-sm font-bold text-theme-primary uppercase tracking-wider">Asset Library</span>
            </div>
          </div>
          
          <div className="p-3 border-b border-theme-glass-light glass-card-sm">
            <div className="relative">
              <Search className="w-4 h-4 absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted" />
              <input type="text" placeholder="Search modules..." className="glass-input w-full pl-10 py-2 text-xs" />
            </div>
          </div>

          <div className="flex-1 overflow-y-auto p-3 space-y-2 custom-scrollbar">
            {[
              { name: 'Initial Access', count: 14, color: 'text-theme-accent-secondary' },
              { name: 'PrivEsc Toolkit', count: 28, color: 'text-theme-accent' },
              { name: 'Lateral Movement', count: 12, color: 'text-theme-danger' },
              { name: 'Data Exfiltration', count: 8, color: 'text-theme-accent' },
              { name: 'Post-Exploitation', count: 42, color: 'text-theme-accent-secondary opacity-80' },
            ].map((category) => (
              <div key={category.name} className="p-3 glass-card-sm border border-theme-glass-light rounded-xl hover:border-theme-accent transition-all cursor-pointer group">
                <div className="flex items-center justify-between mb-2">
                  <p className="text-xs font-bold text-theme-secondary group-hover:text-theme-primary transition-colors">{category.name}</p>
                  <span className={cn("text-[10px] font-black font-mono", category.color)}>{category.count}</span>
                </div>
                <div className="h-1 w-full bg-theme-glass rounded-full overflow-hidden">
                  <div className={cn("h-full opacity-60", category.color.replace('text-', 'bg-'))} style={{ width: `${(category.count / 50) * 100}%` }} />
                </div>
              </div>
            ))}
          </div>

          <div className="glass-card-sm px-4 py-3 border-t border-theme-glass-light">
            <span className="font-mono text-[10px] text-theme-muted tracking-widest uppercase">VER: 1.0.4-TACTICAL</span>
          </div>
        </div>
      </div>
    </div>
  );
};

export default Tactical;
