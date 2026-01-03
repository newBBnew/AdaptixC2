import React, { useState } from 'react';
import { Settings as SettingsIcon, Monitor, Layout, Bell, Save, Palette, Check } from 'lucide-react';
import { cn } from '../../utils/cn';
import { useTheme } from '../../context/ThemeContext';

const Settings = () => {
  const [activeTab, setActiveTab] = useState('theme');
  const { currentTheme, themeList, switchTheme } = useTheme();
  
  // Settings state (aligned with Qt DialogSettings)
  const [settings, setSettings] = useState({
    // Main settings
    fontFamily: 'DejaVu Sans Mono',
    fontSize: 12,
    graphVersion: 'Version 1',
    terminalBufferLines: 10000,
    consoleBufferLines: 100000,
    consolePrintTime: true,
    consoleNoWrap: false,
    consoleAutoScroll: true,
    // Sessions columns
    sessionsColumns: {
      agentId: true, agentType: true, external: true, listener: true,
      internal: true, domain: true, computer: true, user: true,
      os: true, process: true, pid: true, tid: true,
      tags: true, created: true, last: true, sleep: true
    },
    sessionsHealthCheck: true,
    sessionsCoaf: 1.5,
    sessionsOffset: 30,
    // Tasks columns
    tasksColumns: {
      taskId: true, taskType: true, agentId: true, client: true, user: true,
      computer: true, startTime: true, finishTime: true, commandline: true,
      result: true, output: true
    },
    // Tab blink
    tabBlinkEnabled: true,
    tabBlinkItems: { chat: true, logs: true, tasks: false }
  });

  const fonts = ['DejaVu Sans Mono', 'Droid Sans Mono', 'VT323', 'Hack', 'Anonymous Pro', 'Space Mono'];
  const graphVersions = ['Version 1', 'Version 2', 'Version 3'];

  const tabs = [
    { id: 'theme', label: 'Theme', icon: Palette },
    { id: 'main', label: 'Main settings', icon: SettingsIcon },
    { id: 'sessions', label: 'Sessions table', icon: Monitor },
    { id: 'tasks', label: 'Tasks table', icon: Layout },
    { id: 'tabblink', label: 'Blinking tabs', icon: Bell },
  ];

  const sessionsColumnLabels = [
    'Agent ID', 'Agent Type', 'External', 'Listener', 'Internal',
    'Domain', 'Computer', 'User', 'OS', 'Process', 'PID', 'TID', 'Tags', 'Created', 'Last', 'Sleep'
  ];

  const tasksColumnLabels = [
    'Task ID', 'Task Type', 'Agent ID', 'Client', 'User',
    'Computer', 'Start Time', 'Finish Time', 'Commandline', 'Result', 'Output'
  ];

  const handleSave = () => {
    localStorage.setItem('adaptix_settings', JSON.stringify(settings));
    alert('Settings saved!');
  };

  return (
    <div className="flex h-full w-full overflow-hidden select-none">
      {/* Sidebar */}
      <div className="w-44 glass-panel border-r border-theme-glass flex flex-col shrink-0">
        <div className="p-3 border-b border-theme-glass-light glass-card-sm">
          <h1 className="text-[11px] font-black text-theme-secondary uppercase tracking-[0.2em]">Application Config</h1>
        </div>
        <div className="flex-1 p-1 space-y-0.5 overflow-y-auto custom-scrollbar">
          {tabs.map(tab => (
            <button
              key={tab.id}
              onClick={() => setActiveTab(tab.id)}
              className={cn(
                "w-full flex items-center space-x-3 px-3 py-2 transition-all duration-150 border-l-2 rounded-r-lg",
                activeTab === tab.id 
                  ? "glass-card-sm text-theme-primary border-l-theme-accent" 
                  : "text-theme-muted border-transparent hover:bg-theme-hover hover:text-theme-primary"
              )}
            >
              <tab.icon size={14} className={activeTab === tab.id ? "text-theme-accent" : "text-theme-muted"} />
              <span className="text-[11px] font-bold uppercase tracking-tight">{tab.label}</span>
            </button>
          ))}
        </div>
        <div className="p-2 border-t border-theme-glass-light glass-card-sm">
          <button 
            onClick={handleSave}
            className="glass-btn w-full flex items-center justify-center space-x-2 py-2"
          >
            <Save size={12} className="text-theme-accent" />
            <span className="font-bold uppercase tracking-widest text-[10px] text-theme-primary">Apply Changes</span>
          </button>
        </div>
      </div>

      {/* Content */}
      <div className="flex-1 flex flex-col overflow-hidden glass-panel">
        <div className="glass-card-sm px-6 py-3 shrink-0 flex items-center justify-between border-b border-theme-glass-light">
          <div className="flex items-center space-x-3">
            {React.createElement(tabs.find(t => t.id === activeTab)?.icon, { size: 16, className: "text-theme-accent" })}
            <h2 className="text-xs font-black uppercase tracking-widest text-theme-primary">{tabs.find(t => t.id === activeTab)?.label}</h2>
          </div>
          <span className="glass-btn px-2 py-1 text-xs text-theme-accent-secondary">LOCAL</span>
        </div>

        <div className="flex-1 p-6 overflow-auto custom-scrollbar">
          {activeTab === 'theme' && (
            <div className="space-y-6 max-w-4xl animate-in fade-in duration-300">
              <div className="glass-panel rounded-xl p-6">
                <div className="flex items-center space-x-3 mb-6">
                  <Palette size={20} className="text-theme-accent" />
                  <div>
                    <h3 className="text-lg font-semibold text-theme-primary">选择主题</h3>
                    <p className="text-sm text-theme-muted">选择您喜欢的界面风格</p>
                  </div>
                </div>
                
                <div className="grid grid-cols-2 lg:grid-cols-3 gap-4">
                  {themeList.map((theme) => (
                    <button
                      key={theme.id}
                      onClick={() => switchTheme(theme.id)}
                      className={cn(
                        "relative p-4 rounded-xl border-2 transition-all duration-300 text-left group",
                        currentTheme === theme.id 
                          ? "border-theme-accent bg-theme-glass shadow-lg" 
                          : "border-theme-glass hover:border-theme-accent hover:bg-theme-hover"
                      )}
                    >
                      {/* Theme preview */}
                      <div className={cn(
                        "h-20 rounded-lg mb-3 overflow-hidden",
                        theme.colors.background
                      )}>
                        <div className="h-full p-2 flex flex-col space-y-1">
                          <div className={cn("h-3 rounded", theme.colors.glassCard)} />
                          <div className="flex-1 flex space-x-1">
                            <div className={cn("w-1/3 rounded", theme.colors.glassPanel)} />
                            <div className={cn("flex-1 rounded", theme.colors.glassPanel)} />
                          </div>
                        </div>
                      </div>
                      
                      <div className="flex items-center justify-between">
                        <div>
                          <h4 className="font-semibold text-theme-primary">{theme.name}</h4>
                          <p className="text-xs text-theme-muted">{theme.description}</p>
                        </div>
                        {currentTheme === theme.id && (
                          <div className="w-6 h-6 rounded-full bg-theme-accent flex items-center justify-center">
                            <Check size={14} className="text-white" />
                          </div>
                        )}
                      </div>
                    </button>
                  ))}
                </div>
              </div>
              
              <div className="glass-panel rounded-xl p-6">
                <div className="flex items-center space-x-3 mb-4">
                  <Monitor size={20} className="text-theme-accent" />
                  <div>
                    <h3 className="text-lg font-semibold text-theme-primary">当前主题</h3>
                    <p className="text-sm text-theme-muted">您正在使用的主题配置</p>
                  </div>
                </div>
                
                <div className="glass-card-sm rounded-lg p-4">
                  <div className="flex items-center space-x-4">
                    <div className={cn(
                      "w-16 h-16 rounded-lg",
                      themeList.find(t => t.id === currentTheme)?.colors.background
                    )} />
                    <div>
                      <h4 className="font-bold text-theme-primary text-lg">
                        {themeList.find(t => t.id === currentTheme)?.name}
                      </h4>
                      <p className="text-sm text-theme-muted">
                        {themeList.find(t => t.id === currentTheme)?.description}
                      </p>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          )}

          {activeTab === 'main' && (
            <div className="space-y-6 max-w-2xl animate-in fade-in duration-300">
              <div className="grid grid-cols-2 gap-6">
                <div className="space-y-1.5">
                  <label className="block text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">Interface Font</label>
                  <select 
                    value={settings.fontFamily}
                    onChange={(e) => setSettings(s => ({...s, fontFamily: e.target.value}))}
                    className="glass-input w-full py-2 px-3"
                  >
                    {fonts.map(f => <option key={f} value={f}>Adaptix - {f}</option>)}
                  </select>
                </div>
                <div className="space-y-1.5">
                  <label className="block text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">Font Size (px)</label>
                  <input 
                    type="number" min={7} max={30}
                    value={settings.fontSize}
                    onChange={(e) => setSettings(s => ({...s, fontSize: parseInt(e.target.value)}))}
                    className="glass-input w-full font-mono text-center py-2"
                  />
                </div>
                <div className="space-y-1.5">
                  <label className="block text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">Graph Engine</label>
                  <select 
                    value={settings.graphVersion}
                    onChange={(e) => setSettings(s => ({...s, graphVersion: e.target.value}))}
                    className="glass-input w-full py-2 px-3"
                  >
                    {graphVersions.map(v => <option key={v} value={v}>{v}</option>)}
                  </select>
                </div>
              </div>

              <div className="glass-panel rounded-xl overflow-hidden">
                <div className="glass-card-sm px-4 py-2 border-b border-theme-glass-light">
                  <div className="flex items-center space-x-2">
                    <Monitor size={14} className="text-theme-accent" />
                    <span className="text-sm font-semibold text-theme-primary">Agent Console Defaults</span>
                  </div>
                </div>
                <div className="p-4 space-y-4">
                  <div className="w-1/2 space-y-1.5">
                    <label className="block text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">History Buffer Limit</label>
                    <input 
                      type="number" min={10000} max={1000000}
                      value={settings.consoleBufferLines}
                      onChange={(e) => setSettings(s => ({...s, consoleBufferLines: parseInt(e.target.value)}))}
                      className="glass-input w-full font-mono py-2 px-3"
                    />
                  </div>
                  <div className="space-y-2 pt-2 border-t border-theme-glass-light">
                    {[
                      { key: 'consolePrintTime', label: 'Include timestamps in stream' },
                      { key: 'consoleNoWrap', label: 'Disable automatic line wrapping' },
                      { key: 'consoleAutoScroll', label: 'Force scroll to bottom on new data' },
                    ].map(item => (
                      <label key={item.key} className="flex items-center space-x-3 cursor-pointer group">
                        <input 
                          type="checkbox" 
                          checked={settings[item.key]} 
                          onChange={(e) => setSettings(s => ({...s, [item.key]: e.target.checked}))} 
                          className="sr-only" 
                        />
                        <div className={cn(
                          "w-4 h-4 border rounded flex items-center justify-center transition-colors",
                          settings[item.key] ? "bg-theme-accent border-theme-accent" : "border-theme-glass bg-theme-glass-panel group-hover:border-theme-glass"
                        )}>
                          {settings[item.key] && <div className="w-2 h-2 bg-white rounded-full" />}
                        </div>
                        <span className="text-sm font-medium text-theme-secondary group-hover:text-theme-primary">{item.label}</span>
                      </label>
                    ))}
                  </div>
                </div>
              </div>
            </div>
          )}

          {activeTab === 'sessions' && (
            <div className="space-y-6 max-w-2xl animate-in fade-in duration-300 text-left">
              <div className="glass-panel overflow-hidden rounded-2xl border border-theme-glass-light">
                <div className="bg-theme-glass px-4 py-2 border-b border-theme-glass-light">
                  <div className="flex items-center space-x-2">
                    <Layout size={14} className="text-theme-accent" />
                    <span className="text-[10px] font-black uppercase text-theme-primary tracking-widest">Display Configuration</span>
                  </div>
                </div>
                <div className="p-6 space-y-4 bg-theme-glass-panel">
                  <label className="flex items-center space-x-3 cursor-pointer group">
                    <input type="checkbox" checked={settings.sessionsShowGraph} onChange={(e) => setSettings(s => ({...s, sessionsShowGraph: e.target.checked}))} className="sr-only" />
                    <div className={cn("w-5 h-5 border border-theme-glass-light rounded-lg flex items-center justify-center transition-all", settings.sessionsShowGraph ? "bg-theme-accent border-theme-accent shadow-glow-sm" : "bg-theme-glass group-hover:border-theme-accent/50")}>
                      {settings.sessionsShowGraph && <Check size={14} className="text-white" />}
                    </div>
                    <span className="text-xs font-black uppercase text-theme-secondary tracking-widest">Show infrastructure topology graph</span>
                  </label>
                </div>
              </div>
              
              <div className="glass-panel p-6 bg-theme-glass-panel rounded-2xl border border-theme-glass-light flex items-center space-x-8">
                <label className="flex items-center space-x-3 cursor-pointer group">
                  <input type="checkbox" checked={settings.sessionsHealthCheck} onChange={(e) => setSettings(s => ({...s, sessionsHealthCheck: e.target.checked}))} className="sr-only" />
                  <div className={cn("w-5 h-5 border border-theme-glass-light rounded-lg flex items-center justify-center transition-all", settings.sessionsHealthCheck ? "bg-theme-accent border-theme-accent shadow-glow-sm" : "bg-theme-glass group-hover:border-theme-accent/50")}>
                    {settings.sessionsHealthCheck && <Check size={14} className="text-white" />}
                  </div>
                  <span className="text-xs font-black uppercase text-theme-secondary tracking-widest">Enable node health monitor</span>
                </label>

                <div className="flex items-center space-x-4 text-[11px] font-black text-theme-muted uppercase tracking-widest">
                  <span>Inactivity Threshold:</span>
                  <div className="flex items-center space-x-2">
                    <input type="number" step="0.1" min={1} max={5} value={settings.sessionsCoaf} onChange={(e) => setSettings(s => ({...s, sessionsCoaf: parseFloat(e.target.value)}))} className="glass-input w-16 text-center py-1 font-mono text-theme-accent" />
                    <span>* T +</span>
                    <input type="number" min={1} max={10000} value={settings.sessionsOffset} onChange={(e) => setSettings(s => ({...s, sessionsOffset: parseInt(e.target.value)}))} className="glass-input w-20 text-center py-1 font-mono text-theme-accent" />
                    <span className="ml-1 opacity-60">SEC</span>
                  </div>
                </div>
              </div>
            </div>
          )}

          {activeTab === 'tasks' && (
            <div className="space-y-6 max-w-2xl animate-in fade-in duration-300 text-left">
              <div className="glass-panel overflow-hidden rounded-2xl border border-theme-glass-light">
                <div className="bg-theme-glass px-4 py-2 border-b border-theme-glass-light">
                  <div className="flex items-center space-x-2">
                    <Layout size={14} className="text-theme-accent" />
                    <span className="text-[10px] font-black uppercase text-theme-primary tracking-widest">Task Buffer Config</span>
                  </div>
                </div>
                <div className="p-6 space-y-4 bg-theme-glass-panel">
                  <label className="flex items-center space-x-3 cursor-pointer group">
                    <input type="checkbox" checked={settings.tasksShowAll} onChange={(e) => setSettings(s => ({...s, tasksShowAll: e.target.checked}))} className="sr-only" />
                    <div className={cn("w-5 h-5 border border-theme-glass-light rounded-lg flex items-center justify-center transition-all", settings.tasksShowAll ? "bg-theme-accent border-theme-accent shadow-glow-sm" : "bg-theme-glass group-hover:border-theme-accent/50")}>
                      {settings.tasksShowAll && <Check size={14} className="text-white" />}
                    </div>
                    <span className="text-xs font-black uppercase text-theme-secondary tracking-widest">Retain global command history</span>
                  </label>
                </div>
              </div>

              <label className="flex items-center space-x-3 cursor-pointer group p-4 glass-card-sm border-theme-glass-light rounded-2xl hover:border-theme-accent/30 transition-all">
                <input type="checkbox" checked={settings.tasksBlink} onChange={(e) => setSettings(s => ({...s, tasksBlink: e.target.checked}))} className="sr-only" />
                <div className={cn("w-5 h-5 border border-theme-glass-light rounded-lg flex items-center justify-center transition-all", settings.tasksBlink ? "bg-theme-accent-secondary border-theme-accent-secondary shadow-glow-sm" : "bg-theme-glass group-hover:border-theme-accent-secondary/50")}>
                  {settings.tasksBlink && <Check size={14} className="text-white" />}
                </div>
                <span className="text-xs font-black uppercase text-theme-primary tracking-widest">Activate tab notification blink</span>
              </label>

              <div className="glass-panel overflow-hidden rounded-2xl border border-theme-glass-light opacity-90">
                <div className="bg-theme-glass px-4 py-2 border-b border-theme-glass-light">
                  <div className="flex items-center space-x-2">
                    <Bell size={14} className="text-theme-accent-secondary" />
                    <span className="text-[10px] font-black uppercase text-theme-primary tracking-widest">Notification Channels</span>
                  </div>
                </div>
                <div className="p-4 bg-theme-glass-panel grid grid-cols-2 gap-y-2 gap-x-8">
                  {['Chat', 'Logs', 'Sessions', 'Tasks', 'Downloads', 'Credentials', 'Targets', 'Screenshots'].map(tab => (
                    <label key={tab} className="flex items-center space-x-3 cursor-pointer group">
                      <input type="checkbox" checked={true} disabled={!settings.tasksBlink} readOnly className="sr-only" />
                      <div className={cn("w-5 h-5 border border-theme-glass-light rounded-lg flex items-center justify-center transition-all", settings.tasksBlink ? "bg-theme-accent-secondary/50 border-theme-accent-secondary/50" : "bg-theme-glass border-theme-glass-panel group-hover:border-theme-accent-secondary/50")}>
                        {settings.tasksBlink && <Check size={14} className="text-white" />}
                      </div>
                      <span className={cn("text-[11px] font-bold uppercase tracking-tight", settings.tasksBlink ? "text-theme-secondary group-hover:text-theme-primary" : "text-theme-muted")}>{tab}</span>
                    </label>
                  ))}
                </div>
              </div>
            </div>
          )}

          {activeTab === 'tabblink' && (
            <div className="space-y-6 max-w-2xl animate-in fade-in duration-300 text-left">
              <label className="flex items-center space-x-3 cursor-pointer group p-4 glass-card-sm border-theme-glass-light rounded-2xl hover:border-theme-accent/30 transition-all">
                <input type="checkbox" checked={settings.tabBlinkEnabled} onChange={(e) => setSettings(s => ({...s, tabBlinkEnabled: e.target.checked}))} className="sr-only" />
                <div className={cn("w-5 h-5 border border-theme-glass-light rounded-lg flex items-center justify-center transition-all", settings.tabBlinkEnabled ? "bg-theme-accent border-theme-accent shadow-glow-sm" : "bg-theme-glass group-hover:border-theme-accent/50")}>
                  {settings.tabBlinkEnabled && <Check size={14} className="text-white" />}
                </div>
                <span className="text-xs font-black uppercase text-theme-primary tracking-widest">Activate tab notification blink</span>
              </label>

              <div className="glass-panel overflow-hidden rounded-2xl border border-theme-glass-light opacity-90">
                <div className="bg-theme-glass px-4 py-2 border-b border-theme-glass-light">
                  <div className="flex items-center space-x-2">
                    <Bell size={14} className="text-theme-accent-secondary" />
                    <span className="text-[10px] font-black uppercase text-theme-primary tracking-widest">Monitored Tab Sources</span>
                  </div>
                </div>
                <div className="p-4 bg-theme-glass-panel grid grid-cols-2 gap-y-2 gap-x-8">
                  {['Chat', 'Logs', 'Sessions', 'Tasks', 'Downloads', 'Credentials', 'Targets', 'Screenshots'].map(tab => (
                    <label key={tab} className="flex items-center space-x-3 cursor-pointer group">
                      <input type="checkbox" checked={true} disabled={!settings.tabBlinkEnabled} readOnly className="sr-only" />
                      <div className={cn("w-5 h-5 border border-theme-glass-light rounded-lg flex items-center justify-center transition-all", settings.tabBlinkEnabled ? "bg-theme-accent-secondary/50 border-theme-accent-secondary/50" : "bg-theme-glass border-theme-glass-panel group-hover:border-theme-accent-secondary/50")}>
                        {settings.tabBlinkEnabled && <Check size={14} className="text-white" />}
                      </div>
                      <span className={cn("text-[11px] font-bold uppercase tracking-tight", settings.tabBlinkEnabled ? "text-theme-secondary group-hover:text-theme-primary" : "text-theme-muted")}>{tab}</span>
                    </label>
                  ))}
                </div>
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

export default Settings;
