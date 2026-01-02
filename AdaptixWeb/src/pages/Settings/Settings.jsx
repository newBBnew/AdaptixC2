import React, { useState } from 'react';
import { Settings as SettingsIcon, Monitor, Layout, Bell, Save } from 'lucide-react';
import { cn } from '../../utils/cn';

const Settings = () => {
  const [activeTab, setActiveTab] = useState('main');
  
  // Settings state (aligned with Qt DialogSettings)
  const [settings, setSettings] = useState({
    // Main settings
    theme: 'Dark',
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

  const themes = ['Dark', 'Light', 'Dracula', 'Fallout', 'Dark_Old', 'Light_Arc'];
  const fonts = ['DejaVu Sans Mono', 'Droid Sans Mono', 'VT323', 'Hack', 'Anonymous Pro', 'Space Mono'];
  const graphVersions = ['Version 1', 'Version 2', 'Version 3'];

  const tabs = [
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
    <div className="flex h-full w-full bg-dark-900 text-gray-300 overflow-hidden">
      {/* Sidebar */}
      <div className="w-48 bg-dark-800 border-r border-dark-700 flex flex-col">
        <div className="p-4 border-b border-dark-700">
          <h1 className="text-sm font-bold text-white uppercase tracking-wider">Settings</h1>
        </div>
        <div className="flex-1 p-2 space-y-1">
          {tabs.map(tab => (
            <button
              key={tab.id}
              onClick={() => setActiveTab(tab.id)}
              className={cn(
                "w-full flex items-center space-x-2 px-3 py-2 rounded text-left text-[11px] font-medium transition-colors",
                activeTab === tab.id 
                  ? "bg-accent-primary/20 text-accent-primary" 
                  : "text-gray-400 hover:bg-dark-700 hover:text-white"
              )}
            >
              <tab.icon size={14} />
              <span>{tab.label}</span>
            </button>
          ))}
        </div>
        <div className="p-3 border-t border-dark-700">
          <button 
            onClick={handleSave}
            className="w-full flex items-center justify-center space-x-2 px-3 py-2 rounded bg-accent-primary text-white text-[11px] font-bold uppercase hover:bg-accent-primary/80 transition-colors"
          >
            <Save size={14} />
            <span>Apply</span>
          </button>
        </div>
      </div>

      {/* Content */}
      <div className="flex-1 p-6 overflow-auto">
        <h2 className="text-lg font-bold text-white mb-1">{tabs.find(t => t.id === activeTab)?.label}</h2>
        <div className="h-px bg-dark-700 mb-6" />

        {activeTab === 'main' && (
          <div className="space-y-6 max-w-2xl">
            <div className="grid grid-cols-2 gap-4">
              <div>
                <label className="block text-[10px] font-bold uppercase text-gray-500 mb-1">Main theme</label>
                <select 
                  value={settings.theme}
                  onChange={(e) => setSettings(s => ({...s, theme: e.target.value}))}
                  className="w-full bg-dark-800 border border-dark-600 rounded px-3 py-2 text-sm outline-none focus:border-accent-primary"
                >
                  {themes.map(t => <option key={t} value={t}>{t}</option>)}
                </select>
              </div>
              <div>
                <label className="block text-[10px] font-bold uppercase text-gray-500 mb-1">Font family</label>
                <select 
                  value={settings.fontFamily}
                  onChange={(e) => setSettings(s => ({...s, fontFamily: e.target.value}))}
                  className="w-full bg-dark-800 border border-dark-600 rounded px-3 py-2 text-sm outline-none focus:border-accent-primary"
                >
                  {fonts.map(f => <option key={f} value={f}>Adaptix - {f}</option>)}
                </select>
              </div>
              <div>
                <label className="block text-[10px] font-bold uppercase text-gray-500 mb-1">Font size</label>
                <input 
                  type="number" min={7} max={30}
                  value={settings.fontSize}
                  onChange={(e) => setSettings(s => ({...s, fontSize: parseInt(e.target.value)}))}
                  className="w-full bg-dark-800 border border-dark-600 rounded px-3 py-2 text-sm outline-none focus:border-accent-primary"
                />
              </div>
              <div>
                <label className="block text-[10px] font-bold uppercase text-gray-500 mb-1">Session Graph version</label>
                <select 
                  value={settings.graphVersion}
                  onChange={(e) => setSettings(s => ({...s, graphVersion: e.target.value}))}
                  className="w-full bg-dark-800 border border-dark-600 rounded px-3 py-2 text-sm outline-none focus:border-accent-primary"
                >
                  {graphVersions.map(v => <option key={v} value={v}>{v}</option>)}
                </select>
              </div>
            </div>

            <div className="bg-dark-800 border border-dark-700 rounded-lg p-4">
              <h3 className="text-sm font-bold text-white mb-3">Agent Console</h3>
              <div className="grid grid-cols-2 gap-4">
                <div>
                  <label className="block text-[10px] font-bold uppercase text-gray-500 mb-1">Buffer size (lines)</label>
                  <input 
                    type="number" min={10000} max={1000000}
                    value={settings.consoleBufferLines}
                    onChange={(e) => setSettings(s => ({...s, consoleBufferLines: parseInt(e.target.value)}))}
                    className="w-full bg-dark-900 border border-dark-600 rounded px-3 py-2 text-sm outline-none focus:border-accent-primary"
                  />
                </div>
              </div>
              <div className="mt-3 space-y-2">
                <label className="flex items-center space-x-2 cursor-pointer">
                  <input type="checkbox" checked={settings.consolePrintTime} onChange={(e) => setSettings(s => ({...s, consolePrintTime: e.target.checked}))} className="rounded" />
                  <span className="text-sm">Print date and time</span>
                </label>
                <label className="flex items-center space-x-2 cursor-pointer">
                  <input type="checkbox" checked={settings.consoleNoWrap} onChange={(e) => setSettings(s => ({...s, consoleNoWrap: e.target.checked}))} className="rounded" />
                  <span className="text-sm">No Wrap mode</span>
                </label>
                <label className="flex items-center space-x-2 cursor-pointer">
                  <input type="checkbox" checked={settings.consoleAutoScroll} onChange={(e) => setSettings(s => ({...s, consoleAutoScroll: e.target.checked}))} className="rounded" />
                  <span className="text-sm">Auto Scroll mode</span>
                </label>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'sessions' && (
          <div className="space-y-6 max-w-2xl">
            <div className="bg-dark-800 border border-dark-700 rounded-lg p-4">
              <h3 className="text-sm font-bold text-white mb-3">Columns</h3>
              <div className="grid grid-cols-2 gap-2">
                {sessionsColumnLabels.map((label, i) => {
                  const key = label.replace(/\s/g, '').replace(/^./, c => c.toLowerCase());
                  return (
                    <label key={i} className="flex items-center space-x-2 cursor-pointer text-sm">
                      <input type="checkbox" checked={true} className="rounded" />
                      <span>{label}</span>
                    </label>
                  );
                })}
              </div>
            </div>
            <div className="flex items-center space-x-4">
              <label className="flex items-center space-x-2 cursor-pointer">
                <input type="checkbox" checked={settings.sessionsHealthCheck} onChange={(e) => setSettings(s => ({...s, sessionsHealthCheck: e.target.checked}))} className="rounded" />
                <span className="text-sm">Check Health</span>
              </label>
              <div className="flex items-center space-x-2 text-sm">
                <span>Sleeptime *</span>
                <input type="number" step="0.1" min={1} max={5} value={settings.sessionsCoaf} onChange={(e) => setSettings(s => ({...s, sessionsCoaf: parseFloat(e.target.value)}))} className="w-16 bg-dark-800 border border-dark-600 rounded px-2 py-1 text-sm" />
                <span>+</span>
                <input type="number" min={1} max={10000} value={settings.sessionsOffset} onChange={(e) => setSettings(s => ({...s, sessionsOffset: parseInt(e.target.value)}))} className="w-20 bg-dark-800 border border-dark-600 rounded px-2 py-1 text-sm" />
                <span>sec</span>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'tasks' && (
          <div className="space-y-6 max-w-2xl">
            <div className="bg-dark-800 border border-dark-700 rounded-lg p-4">
              <h3 className="text-sm font-bold text-white mb-3">Columns</h3>
              <div className="grid grid-cols-2 gap-2">
                {tasksColumnLabels.map((label, i) => (
                  <label key={i} className="flex items-center space-x-2 cursor-pointer text-sm">
                    <input type="checkbox" checked={true} className="rounded" />
                    <span>{label}</span>
                  </label>
                ))}
              </div>
            </div>
          </div>
        )}

        {activeTab === 'tabblink' && (
          <div className="space-y-6 max-w-2xl">
            <label className="flex items-center space-x-2 cursor-pointer">
              <input type="checkbox" checked={settings.tabBlinkEnabled} onChange={(e) => setSettings(s => ({...s, tabBlinkEnabled: e.target.checked}))} className="rounded" />
              <span className="text-sm font-bold">Enable tab blink</span>
            </label>
            <div className="bg-dark-800 border border-dark-700 rounded-lg p-4">
              <h3 className="text-sm font-bold text-white mb-3">Blinking tabs</h3>
              <div className="grid grid-cols-2 gap-2">
                {['Chat', 'Logs', 'Sessions', 'Tasks', 'Downloads', 'Credentials', 'Targets', 'Screenshots'].map(tab => (
                  <label key={tab} className="flex items-center space-x-2 cursor-pointer text-sm">
                    <input type="checkbox" checked={true} disabled={!settings.tabBlinkEnabled} className="rounded" />
                    <span>{tab}</span>
                  </label>
                ))}
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default Settings;
