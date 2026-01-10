import React, { useState, useMemo } from 'react';
import { Crosshair, Box, Layout, Shield, Zap, Terminal, Search, Plus, ChevronRight, Play } from 'lucide-react';
import { cn } from '../../utils/cn';
import { useAgents } from '../../context/AgentContext';
import PluginDialog from '../Agents/PluginDialog';

const Tactical = () => {
  const { axCommands, axPlugins, axStats, agents, openAgentTab, processCommand } = useAgents();
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedPluginCmd, setSelectedPluginCmd] = useState(null);
  const [pluginDialogOpen, setPluginDialogOpen] = useState(false);
  const [targetAgentId, setTargetAgentId] = useState('');
  const [isPluginOpen, setIsPluginOpen] = useState(false);

  // Group commands by their designated group for the Asset Library
  const commandGroups = useMemo(() => {
    return axCommands.reduce((acc, cmd) => {
      const group = cmd.group || 'Misc';
      if (!acc[group]) acc[group] = [];
      acc[group].push(cmd);
      return acc;
    }, {});
  }, [axCommands]);

  const filteredGroups = useMemo(() => {
    if (!searchQuery.trim()) return commandGroups;
    const query = searchQuery.toLowerCase();
    const filtered = {};
    
    Object.entries(commandGroups).forEach(([group, cmds]) => {
      const matchingCmds = cmds.filter(c => 
        c.name.toLowerCase().includes(query) || 
        c.description?.toLowerCase().includes(query) ||
        group.toLowerCase().includes(query)
      );
      if (matchingCmds.length > 0) {
        filtered[group] = matchingCmds;
      }
    });
    return filtered;
  }, [commandGroups, searchQuery]);

  const handleCmdClick = (cmd) => {
    setSelectedPluginCmd(cmd);
    setIsPluginOpen(true);
  };

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
              <span className="glass-btn px-2 py-0.5 text-[10px] text-theme-accent-secondary">
                {axStats.commandCount > 0 ? 'ACTIVE' : 'IDLE'}
              </span>
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
              {axStats.commandCount > 0 
                ? `Ready to orchestrate ${axStats.commandCount} discovered capabilities across ${axStats.loadedScripts} active extension kits.`
                : "Waiting for Extension-Kit synchronization from Teamserver gateway..."}
            </p>
            <button className="mt-10 glass-btn-primary px-10 py-3 rounded-xl font-black uppercase tracking-[0.2em] text-sm relative z-10 transition-all active:scale-95">
              Open Framework Browser
            </button>
          </div>
        </div>

        {/* Right Sidebar: Asset Library */}
        <div className="glass-panel overflow-hidden rounded-2xl border border-theme-glass-light opacity-90">
          <div className="glass-card-sm px-4 py-3 flex items-center border-b border-theme-glass-light">
            <div className="flex items-center space-x-2">
              <Shield size={16} className="text-theme-danger" />
              <span className="text-sm font-bold text-theme-primary uppercase tracking-wider">Asset Library</span>
            </div>
          </div>
          
          <div className="p-3 border-b border-theme-glass-light glass-card-sm">
            <div className="relative">
              <Search className="w-4 h-4 absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted" />
              <input 
                type="text" 
                placeholder="Search modules..." 
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                className="glass-input w-full pl-10 py-2 text-xs" 
              />
            </div>
          </div>

          <div className="flex-1 overflow-y-auto p-3 space-y-4 custom-scrollbar">
            {Object.keys(filteredGroups).length === 0 ? (
              <div className="text-center p-8 text-[10px] text-theme-muted uppercase font-bold tracking-widest opacity-50">
                No matches found
              </div>
            ) : (
              Object.entries(filteredGroups).map(([group, cmds]) => (
                <div key={group} className="space-y-2">
                  <div className="flex items-center justify-between px-1">
                    <p className="text-[9px] font-black text-theme-muted uppercase tracking-[0.2em]">{group}</p>
                    <span className="text-[9px] font-mono text-theme-accent opacity-50">{cmds.length}</span>
                  </div>
                  <div className="space-y-1">
                    {cmds.map(cmd => (
                      <button 
                        key={cmd.name}
                        onClick={() => handleCmdClick(cmd)}
                        className="w-full p-2.5 glass-card-sm border border-theme-glass-light rounded-xl hover:border-theme-accent transition-all text-left group"
                      >
                        <div className="flex items-center justify-between mb-1">
                          <p className="text-xs font-bold text-theme-secondary group-hover:text-theme-primary transition-colors truncate">{cmd.name}</p>
                          <ChevronRight size={12} className="text-theme-muted group-hover:text-theme-accent transition-transform group-hover:translate-x-0.5" />
                        </div>
                        {cmd.description && (
                          <p className="text-[9px] text-theme-muted line-clamp-1 italic">{cmd.description}</p>
                        )}
                      </button>
                    ))}
                  </div>
                </div>
              ))
            )}
          </div>

          <div className="glass-card-sm px-4 py-3 border-t border-theme-glass-light">
            <span className="font-mono text-[10px] text-theme-muted tracking-widest uppercase">VER: 1.0.4-WEB-SYNC</span>
          </div>
        </div>
      </div>
    </div>
  );
};

export default Tactical;
