import React from 'react';
import { LayoutDashboard, Users, Target, Briefcase, AlertTriangle, Activity, ShieldCheck, Clock, Terminal, Box, RotateCw } from 'lucide-react';
import { useAgents } from '../../context/AgentContext';
import { cn } from '../../utils/cn';

const Dashboard = () => {
  const { agents, listeners, targets, tasks, logs, axStats, reloadScripts } = useAgents();

  const activeTasksCount = Object.values(tasks).filter(t => !t.a_completed).length;
  const activeBeaconsCount = agents.filter(a => {
    const lastTick = a.a_last_tick || 0;
    return (Math.floor(Date.now() / 1000) - lastTick) < 300; // 5 minutes threshold
  }).length;

  const stats = [
    { label: 'Active Beacons', value: activeBeaconsCount.toString(), icon: Users, color: 'text-theme-accent-secondary' },
    { label: 'Live Listeners', value: listeners.length.toString(), icon: ShieldCheck, color: 'text-theme-success' },
    { label: 'Tasks Processing', value: activeTasksCount.toString(), icon: Briefcase, color: 'text-theme-accent' },
    { label: 'Total Targets', value: targets.length.toString(), icon: Target, color: 'text-theme-primary' },
    { label: 'Script Engine', value: (axStats?.loadedScripts || 0).toString(), icon: Box, color: 'text-theme-accent-secondary' },
    { label: 'Extensions', value: (axStats?.commandCount || 0).toString(), icon: Terminal, color: 'text-theme-success' },
  ];

  return (
    <div className="w-full h-full flex flex-col select-none overflow-hidden">
      {/* Header */}
      <header className="px-6 py-4 glass-card-sm border-b border-theme-glass-light shrink-0">
        <div className="flex items-center justify-between">
          <div className="flex items-center space-x-4">
            <div className="p-2.5 bg-theme-glass rounded-xl border border-theme-glass-light text-theme-accent shadow-glow-sm">
              <LayoutDashboard size={24} />
            </div>
            <div>
              <h1 className="text-lg font-black uppercase tracking-[0.2em] text-theme-primary">Operational Overview</h1>
              <p className="text-xs text-theme-muted uppercase font-bold tracking-widest">Real-time C2 infrastructure analytics</p>
            </div>
          </div>
          
          <button 
            onClick={reloadScripts}
            className="glass-btn flex items-center space-x-2 px-4 py-2 text-xs font-black uppercase tracking-widest text-theme-accent hover:text-theme-primary transition-all group"
          >
            <RotateCw size={14} className="group-hover:rotate-180 transition-transform duration-500" />
            <span>Reload Extension-Kit</span>
          </button>
        </div>
      </header>

      <div className="flex-1 overflow-auto p-6 space-y-6 custom-scrollbar">
        {/* Stats Grid */}
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
          {stats.map((stat) => (
            <div key={stat.label} className="glass-panel p-5 flex flex-col justify-between group hover:border-theme-accent transition-all duration-300 rounded-2xl">
              <div className="flex items-center justify-between mb-3">
                <p className="text-xs font-black uppercase text-theme-muted tracking-widest">{stat.label}</p>
                <stat.icon size={16} className="text-theme-muted group-hover:text-theme-accent transition-colors" />
              </div>
              <div className="flex items-baseline space-x-2">
                <p className={cn("text-3xl font-mono font-black tracking-tighter", 
                  stat.label.includes('Active') ? "text-theme-accent-secondary" : 
                  stat.label.includes('Tasks') ? "text-theme-accent" :
                  stat.label.includes('Scripts') ? "text-theme-accent-secondary" :
                  stat.label.includes('Commands') ? "text-theme-success" :
                  stat.label.includes('Alerts') ? "text-theme-danger" : "text-theme-primary"
                )}>{stat.value}</p>
                <span className="text-[10px] text-theme-muted font-bold uppercase tracking-widest">
                  {stat.label.includes('Scripts') || stat.label.includes('Commands') ? 'Objects' : 'Nodes'}
                </span>
              </div>
            </div>
          ))}
        </div>

        {/* Main Content Areas */}
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
          {/* Main Activity Panel */}
          <div className="lg:col-span-2 glass-panel flex flex-col overflow-hidden rounded-2xl border border-theme-glass-light">
            <div className="glass-card-sm px-4 py-3 flex items-center justify-between border-b border-theme-glass-light">
              <div className="flex items-center space-x-2">
                <Activity size={16} className="text-theme-accent" />
                <span className="text-sm font-bold text-theme-primary uppercase tracking-wider">Operational Activity</span>
              </div>
              <span className="glass-btn px-2 py-0.5 text-[10px] text-theme-accent-secondary">INFRA_MAP</span>
            </div>
            <div className="flex-1 flex flex-col items-center justify-center p-12 text-center min-h-[300px]">
              <div className="w-20 h-20 bg-theme-glass rounded-full border border-theme-glass-light flex items-center justify-center mb-6 opacity-40 shadow-glow-sm">
                <ShieldCheck size={40} className="text-theme-accent" />
              </div>
              <p className="text-sm font-black uppercase text-theme-primary tracking-[0.2em]">Global Telemetry Data</p>
              <p className="text-xs text-theme-muted mt-2 uppercase italic max-w-md">
                {agents.length > 0 ? `${agents.length} nodes currently connected to teamserver backbone` : "Infrastructure nodes establishing encrypted channel..."}
              </p>
            </div>
          </div>

          {/* Event Log Side Panel */}
          <div className="glass-panel flex flex-col overflow-hidden rounded-2xl border border-theme-glass-light">
            <div className="glass-card-sm px-4 py-3 flex items-center border-b border-theme-glass-light">
              <div className="flex items-center space-x-2">
                <Clock size={16} className="text-theme-accent-secondary" />
                <span className="text-sm font-bold text-theme-primary uppercase tracking-wider">System Events</span>
              </div>
            </div>
            <div className="flex-1 overflow-auto font-mono text-[11px] divide-y divide-theme-glass-light custom-scrollbar">
              {logs.length === 0 ? (
                <div className="p-12 text-center text-theme-muted uppercase font-black text-[10px] tracking-widest">
                  Waiting for events...
                </div>
              ) : (
                [...logs].reverse().map((log, i) => (
                  <div key={i} className="p-3.5 hover:bg-theme-hover transition-colors group flex space-x-3">
                    <span className="text-theme-muted shrink-0 font-bold">
                      {new Date(log.time * 1000).toLocaleTimeString([], { hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' })}
                    </span>
                    <div className="flex-1 min-w-0">
                      <p className="text-theme-secondary leading-relaxed group-hover:text-theme-primary break-words">
                        <span className="text-theme-accent font-bold">[{String(log.type || 'SYSTEM').toUpperCase()}]</span> {log.content}
                      </p>
                    </div>
                  </div>
                ))
              )}
            </div>
            <div className="px-4 py-1.5 glass-card-sm border-t border-theme-glass-light flex items-center justify-between shrink-0">
              <div className="flex items-center space-x-2 bg-theme-glass-panel px-3 py-1 rounded-lg border border-theme-glass-light shadow-sm">
                <span className="text-[10px] font-black uppercase text-theme-muted tracking-widest">Sync Status:</span>
                <span className={cn("text-[10px] font-black uppercase tracking-widest", agents.length > 0 ? "text-theme-success" : "text-theme-muted")}>
                  {agents.length > 0 ? "VERIFIED" : "STANDBY"}
                </span>
              </div>
              <div className="flex items-center space-x-2 bg-theme-glass-panel px-3 py-1 rounded-lg border border-theme-glass-light shadow-glow-sm">
                <div className={cn("w-2 h-2 rounded-full animate-pulse shadow-glow-sm", agents.length > 0 ? "bg-theme-success" : "bg-theme-muted opacity-40")} />
                <span className={cn("text-[10px] font-black uppercase tracking-widest", agents.length > 0 ? "text-theme-success" : "text-theme-muted")}>
                  {agents.length > 0 ? "ONLINE" : "OFFLINE"}
                </span>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default Dashboard;
