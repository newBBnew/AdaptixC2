import React, { useState, useEffect } from 'react';
import Modal from '../../components/Modal';
import { cn } from '../../utils/cn';
import { processDynamicConfig } from '../../utils/configUtils';
import { agentApi } from '../../api/agent';
import { useAgents } from '../../context/AgentContext';
import DynamicFormRenderer from '../../components/DynamicFormRenderer';
import { 
  Cpu, 
  Save, 
  Download,
  Info,
  Check,
  Shield,
  Zap,
  Terminal,
  Clock,
  Calendar,
  Settings,
  FileCode,
  RefreshCw,
  Globe
} from 'lucide-react';

const GenerateAgentDialog = ({ isOpen, onClose, listenerName, listenerType }) => {
  const { availableAgentTypes } = useAgents();
  const [selectedAgent, setSelectedAgent] = useState('beacon');
  const [profileName, setProfileName] = useState('');
  const [saveAsProfile, setSaveAsProfile] = useState(true);
  const [generating, setGenerating] = useState(false);
  const [customFilename, setCustomFilename] = useState('');
  
  // Dynamic Agent config fields matching backend GenerateConfig
  const [config, setConfig] = useState({
    os: 'windows',
    arch: 'x64',
    format: 'Exe',
    sleep: '5',
    jitter: '10',
    svcname: 'AdaptixAgent',
    is_killdate: false,
    kill_date: '',
    kill_time: '00:00:00',
    is_workingtime: false,
    start_time: '08:00',
    end_time: '17:00',
    // DoH Specific
    transport_mode: 'auto',
    doh_mode: 'recursive',
    dns_resolvers: '8.8.8.8,1.1.1.1,9.9.9.9',
    doh_urls: 'https://cloudflare-dns.com/dns-query,https://dns.google/dns-query'
  });

  const isDoH = listenerType?.toLowerCase().includes('doh');

  // Merge hardcoded types with dynamic ones
  const coreAgentTypes = [
    { id: 'beacon', label: 'Beacon', icon: Zap },
    { id: 'gopher', label: 'Gopher', icon: Terminal },
  ];

  // Merge available agents with core agents
  // Priority: Dynamic > Core
  const displayAgentTypes = React.useMemo(() => {
    const merged = [...coreAgentTypes];
    
    availableAgentTypes.forEach(dynamic => {
      const index = merged.findIndex(core => core.id === dynamic.id);
      const dynamicItem = {
        id: dynamic.id,
        label: dynamic.label || dynamic.id,
        icon: dynamic.id.toLowerCase().includes('gopher') ? Terminal : Zap,
        ui_schema: dynamic.ui_schema
      };

      if (index !== -1) {
        merged[index] = { ...merged[index], ...dynamicItem };
      } else {
        merged.push(dynamicItem);
      }
    });
    
    return merged;
  }, [availableAgentTypes]);

  const currentAgent = displayAgentTypes.find(a => a.id === selectedAgent);
  
  // Resolve schema: It might be a single schema object or a map of schemas by listenerType
  let currentSchema = null;
  if (currentAgent?.ui_schema) {
    if (currentAgent.ui_schema.root) {
      // Single schema
      currentSchema = currentAgent.ui_schema;
    } else if (listenerType && currentAgent.ui_schema[listenerType]) {
      // Map of schemas, pick by listenerType
      currentSchema = currentAgent.ui_schema[listenerType];
    }
  }

  const isDynamic = !!currentSchema;

  useEffect(() => {
    if (listenerName) {
      setProfileName(`${listenerName}_Agent`);
    }
    // Set default kill date to tomorrow
    const tomorrow = new Date();
    tomorrow.setDate(tomorrow.getDate() + 1);
    const dd = String(tomorrow.getDate()).padStart(2, '0');
    const mm = String(tomorrow.getMonth() + 1).padStart(2, '0'); //January is 0!
    const yyyy = tomorrow.getFullYear();
    setConfig(prev => ({ ...prev, kill_date: `${dd}.${mm}.${yyyy}` }));
  }, [listenerName]);

  const handleGenerate = async () => {
    setGenerating(true);
    try {
      let finalConfig = {};

      if (isDynamic) {
        // Use config state directly for dynamic agents but sanitize types
        finalConfig = processDynamicConfig(config, currentSchema);
      } else {
        // Hardcoded mapping
        finalConfig = {
          ...config,
          jitter: parseInt(config.jitter) || 0
        };
      }

      const payload = {
        listener_name: listenerName,
        listener_type: listenerType,
        agent: selectedAgent,
        config: JSON.stringify(finalConfig)
      };

      const response = await agentApi.generate(payload);
      const { message, ok } = response.data;

      if (ok && message) {
        const [nameB64, contentB64] = message.split(':');
        let filename = atob(nameB64);
        if (customFilename.trim()) {
            filename = customFilename.trim();
        }
        const binaryString = atob(contentB64);
        const bytes = new Uint8Array(binaryString.length);
        for (let i = 0; i < binaryString.length; i++) {
          bytes[i] = binaryString.charCodeAt(i);
        }
        
        const blob = new Blob([bytes], { type: 'application/octet-stream' });

        try {
          // Support for modern directory picker
          if (window.showSaveFilePicker) {
            const handle = await window.showSaveFilePicker({
              suggestedName: filename,
              types: [{
                description: 'Agent Payload',
                accept: { 'application/octet-stream': ['.exe', '.bin', '.dll'] }
              }],
            });
            const writable = await handle.createWritable();
            await writable.write(blob);
            await writable.close();
          } else {
            // Fallback for older browsers
            const url = window.URL.createObjectURL(blob);
            const link = document.createElement('a');
            link.href = url;
            link.setAttribute('download', filename);
            document.body.appendChild(link);
            link.click();
            link.remove();
            window.URL.revokeObjectURL(url);
          }
          onClose();
        } catch (err) {
          if (err.name !== 'AbortError') {
            console.error('Save failed:', err);
            alert('Failed to save file: ' + err.message);
          }
        }
      } else {
        alert(message || 'Failed to generate agent');
      }
    } catch (err) {
      console.error('Generation failed:', err);
      alert(err.response?.data?.message || 'Connection error during generation');
    } finally {
      setGenerating(false);
    }
  };

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title="Generate Agent"
      width="max-w-xl"
    >
      <div className="flex flex-col h-full bg-theme-glass-panel">
        {/* Context Header */}
        <div className="px-6 py-4 bg-theme-glass border-b border-theme-glass-light flex items-center justify-between">
          <div className="flex items-center space-x-4 text-left">
            <div className="p-2.5 bg-theme-glass-panel border border-theme-glass-light rounded-xl">
              <Shield className="text-theme-accent" size={20} />
            </div>
            <div>
              <p className="text-[10px] font-black uppercase text-theme-muted tracking-widest">Listener Context</p>
              <p className="text-sm font-mono font-bold text-theme-primary">{listenerName || 'UNSPECIFIED'}</p>
            </div>
          </div>
          <div className="text-right">
            <span className="glass-btn px-3 py-1 rounded-lg text-[10px] font-black uppercase tracking-widest text-theme-accent-secondary">
              {listenerType}
            </span>
          </div>
        </div>

        {isDynamic ? (
          <div className="p-6 space-y-6 overflow-y-auto custom-scrollbar bg-theme-glass-panel min-h-[300px]">
             {/* Agent Type Selection */}
             <div className="space-y-3 text-left">
                <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Agent Type</label>
                <div className="flex space-x-3 overflow-x-auto pb-2 custom-scrollbar">
                  {displayAgentTypes.map((type) => (
                    <button
                      key={type.id}
                      onClick={() => setSelectedAgent(type.id)}
                      className={cn(
                        "flex-1 min-w-[100px] flex items-center justify-center space-x-3 py-3 px-4 rounded-2xl border transition-all uppercase font-black text-[10px] tracking-widest",
                        selectedAgent === type.id 
                          ? "bg-theme-glass border-theme-accent text-theme-accent shadow-glow-sm" 
                          : "glass-btn border-theme-glass text-theme-muted hover:border-theme-accent/50 hover:text-theme-primary"
                      )}
                    >
                      <type.icon size={16} />
                      <span>{type.label}</span>
                    </button>
                  ))}
                </div>
              </div>
              
              <div className="border-t border-theme-glass-light my-4"></div>

              {/* Dynamic Content */}
              <DynamicFormRenderer 
                schema={currentAgent.ui_schema} 
                value={config} 
                onChange={setConfig} 
              />
          </div>
        ) : (
          <div className="p-6 space-y-8 overflow-y-auto custom-scrollbar bg-theme-glass-panel flex-1">
              
              {/* General Settings */}
              <div className="space-y-4">
                <div className="flex items-center space-x-2 text-theme-secondary border-b border-theme-glass-light pb-2">
                    <Settings size={14} />
                    <span className="text-xs font-bold uppercase tracking-wide">General Configuration</span>
                </div>

                {/* Agent Type Selection */}
                <div className="space-y-2 text-left">
                  <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Agent Type</label>
                  <div className="flex space-x-3">
                    {displayAgentTypes.map((type) => (
                      <button
                        key={type.id}
                        onClick={() => setSelectedAgent(type.id)}
                        className={cn(
                          "flex-1 flex items-center justify-center space-x-3 py-2 px-4 rounded-xl border transition-all uppercase font-black text-[10px] tracking-widest",
                          selectedAgent === type.id 
                            ? "bg-theme-glass border-theme-accent text-theme-accent shadow-glow-sm" 
                            : "glass-btn border-theme-glass text-theme-muted hover:border-theme-accent/50 hover:text-theme-primary"
                        )}
                      >
                        <type.icon size={14} />
                        <span>{type.label}</span>
                      </button>
                    ))}
                  </div>
                </div>

                {/* Sleep & Jitter */}
                <div className="grid grid-cols-2 gap-4">
                  <div className="space-y-1 text-left">
                    <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Sleep (s)</label>
                    <input 
                      type="number"
                      value={config.sleep}
                      onChange={e => setConfig({...config, sleep: e.target.value})}
                      className="glass-input w-full py-2 px-3 text-theme-primary font-mono text-xs"
                    />
                  </div>
                  <div className="space-y-1 text-left">
                    <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Jitter (%)</label>
                    <input 
                      type="number"
                      value={config.jitter}
                      onChange={e => setConfig({...config, jitter: e.target.value})}
                      className="glass-input w-full py-2 px-3 text-theme-primary font-mono text-xs"
                    />
                  </div>
                </div>
              </div>

              {/* Build Output */}
              <div className="space-y-4">
                  <div className="flex items-center space-x-2 text-theme-secondary border-b border-theme-glass-light pb-2">
                      <FileCode size={14} />
                      <span className="text-xs font-bold uppercase tracking-wide">Build Output</span>
                  </div>

                  <div className="grid grid-cols-2 gap-4">
                    <div className="space-y-1 text-left">
                      <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Platform</label>
                      <select 
                        value={config.arch}
                        onChange={e => setConfig({...config, arch: e.target.value})}
                        className="glass-input w-full py-2 px-3 text-theme-primary text-xs"
                      >
                        <option value="x64" className="bg-theme-glass-panel text-theme-primary">Windows x64</option>
                        <option value="x86" className="bg-theme-glass-panel text-theme-primary">Windows x86</option>
                      </select>
                    </div>
                    <div className="space-y-1 text-left">
                      <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Format</label>
                      <select 
                        value={config.format}
                        onChange={e => setConfig({...config, format: e.target.value})}
                        className="glass-input w-full py-2 px-3 text-theme-primary text-xs"
                      >
                        <option value="Exe" className="bg-theme-glass-panel text-theme-primary">Executable (.exe)</option>
                        <option value="Service Exe" className="bg-theme-glass-panel text-theme-primary">Service Executable (.exe)</option>
                        <option value="DLL" className="bg-theme-glass-panel text-theme-primary">Dynamic Link (.dll)</option>
                        <option value="Shellcode" className="bg-theme-glass-panel text-theme-primary">Raw Shellcode (.bin)</option>
                      </select>
                    </div>
                  </div>

                  <div className="space-y-1 text-left">
                      <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Payload Filename (Optional)</label>
                      <input 
                        type="text"
                        value={customFilename}
                        onChange={e => setCustomFilename(e.target.value)}
                        placeholder="e.g. update_installer.exe"
                        className="glass-input w-full py-2 px-3 text-theme-accent font-mono text-xs"
                      />
                  </div>

                  {config.format === 'Service Exe' && (
                    <div className="space-y-1 text-left animate-in fade-in slide-in-from-top-1">
                      <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Service Name</label>
                      <input 
                        type="text"
                        value={config.svcname}
                        onChange={e => setConfig({...config, svcname: e.target.value})}
                        placeholder="AdaptixAgent"
                        className="glass-input w-full py-2 px-3 text-theme-primary font-mono text-xs"
                      />
                    </div>
                  )}
              </div>

              {/* Network Settings (DoH Only) */}
              {isDoH && (
                <div className="space-y-4 animate-in fade-in">
                  <div className="flex items-center space-x-2 text-theme-secondary border-b border-theme-glass-light pb-2">
                      <Globe size={14} />
                      <span className="text-xs font-bold uppercase tracking-wide">Network</span>
                  </div>

                  <div className="grid grid-cols-2 gap-4">
                    <div className="space-y-1 text-left">
                      <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Transport</label>
                      <select 
                        value={config.transport_mode}
                        onChange={e => setConfig({...config, transport_mode: e.target.value})}
                        className="glass-input w-full py-2 px-3 text-theme-primary text-xs"
                      >
                        <option value="auto" className="bg-theme-glass-panel text-theme-primary">Auto (Hybrid)</option>
                        <option value="dns" className="bg-theme-glass-panel text-theme-primary">DNS Only</option>
                        <option value="doh" className="bg-theme-glass-panel text-theme-primary">DoH Only</option>
                      </select>
                    </div>
                    <div className="space-y-1 text-left">
                      <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">DoH Mode</label>
                      <select 
                        value={config.doh_mode}
                        onChange={e => setConfig({...config, doh_mode: e.target.value})}
                        className="glass-input w-full py-2 px-3 text-theme-primary text-xs"
                      >
                        <option value="recursive" className="bg-theme-glass-panel text-theme-primary">Recursive</option>
                        <option value="direct" className="bg-theme-glass-panel text-theme-primary">Direct</option>
                      </select>
                    </div>
                  </div>

                  <div className="space-y-1 text-left">
                    <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">DNS Resolvers</label>
                    <input 
                      type="text"
                      value={config.dns_resolvers}
                      onChange={e => setConfig({...config, dns_resolvers: e.target.value})}
                      placeholder="8.8.8.8, 1.1.1.1"
                      className="glass-input w-full py-2 px-3 text-theme-primary font-mono text-xs"
                    />
                  </div>

                  <div className="space-y-1 text-left">
                    <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">DoH URLs</label>
                    <textarea 
                      rows="2"
                      value={config.doh_urls}
                      onChange={e => setConfig({...config, doh_urls: e.target.value})}
                      placeholder="https://cloudflare-dns.com/dns-query"
                      className="glass-input w-full py-2 px-3 text-theme-primary font-mono text-[10px] resize-none"
                    />
                  </div>
                </div>
              )}

              {/* Constraints */}
              <div className="space-y-4">
                  <div className="flex items-center space-x-2 text-theme-secondary border-b border-theme-glass-light pb-2">
                      <Clock size={14} />
                      <span className="text-xs font-bold uppercase tracking-wide">Constraints</span>
                  </div>

                  {/* Kill Date */}
                  <div className="p-3 glass-card-sm border-theme-glass-light rounded-xl space-y-3">
                    <div className="flex items-center justify-between">
                      <span className="text-[10px] font-bold uppercase text-theme-muted tracking-wide">Kill Date</span>
                      <label className="relative inline-flex items-center cursor-pointer">
                        <input type="checkbox" checked={config.is_killdate} onChange={e => setConfig({...config, is_killdate: e.target.checked})} className="sr-only peer" />
                        <div className="w-7 h-4 bg-theme-glass-panel peer-focus:outline-none rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-3 after:w-3 after:transition-all peer-checked:bg-theme-accent"></div>
                      </label>
                    </div>
                    
                    {config.is_killdate && (
                      <div className="grid grid-cols-2 gap-3 animate-in fade-in">
                        <input 
                          type="text"
                          value={config.kill_date}
                          onChange={e => setConfig({...config, kill_date: e.target.value})}
                          placeholder="31.12.2024"
                          className="glass-input w-full py-1.5 px-3 text-theme-primary font-mono text-xs"
                        />
                        <input 
                          type="text"
                          value={config.kill_time}
                          onChange={e => setConfig({...config, kill_time: e.target.value})}
                          placeholder="23:59:59"
                          className="glass-input w-full py-1.5 px-3 text-theme-primary font-mono text-xs"
                        />
                      </div>
                    )}
                  </div>

                  {/* Working Time */}
                  <div className="p-3 glass-card-sm border-theme-glass-light rounded-xl space-y-3">
                    <div className="flex items-center justify-between">
                      <span className="text-[10px] font-bold uppercase text-theme-muted tracking-wide">Working Hours</span>
                      <label className="relative inline-flex items-center cursor-pointer">
                        <input type="checkbox" checked={config.is_workingtime} onChange={e => setConfig({...config, is_workingtime: e.target.checked})} className="sr-only peer" />
                        <div className="w-7 h-4 bg-theme-glass-panel peer-focus:outline-none rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-3 after:w-3 after:transition-all peer-checked:bg-theme-accent"></div>
                      </label>
                    </div>
                    
                    {config.is_workingtime && (
                      <div className="grid grid-cols-2 gap-3 animate-in fade-in">
                        <input 
                          type="text"
                          value={config.start_time}
                          onChange={e => setConfig({...config, start_time: e.target.value})}
                          placeholder="09:00"
                          className="glass-input w-full py-1.5 px-3 text-theme-primary font-mono text-xs"
                        />
                        <input 
                          type="text"
                          value={config.end_time}
                          onChange={e => setConfig({...config, end_time: e.target.value})}
                          placeholder="17:00"
                          className="glass-input w-full py-1.5 px-3 text-theme-primary font-mono text-xs"
                        />
                      </div>
                    )}
                  </div>
              </div>
          </div>
        )}

        {/* Footer Actions */}
        <div className="p-6 border-t border-theme-glass-light flex items-center justify-between bg-theme-glass-panel shrink-0">
          <div className="flex items-center space-x-2 text-theme-muted italic text-[10px] uppercase font-bold tracking-tighter">
            <Info size={14} className="text-theme-accent" />
            <span>Encrypted Compilation via Teamserver</span>
          </div>
          <div className="flex items-center space-x-3">
            <button 
              onClick={onClose}
              className="glass-btn px-6 py-2 text-xs font-bold text-theme-muted hover:text-theme-primary transition-all uppercase tracking-widest"
            >
              Cancel
            </button>
            <button 
              onClick={handleGenerate}
              disabled={generating}
              className="glass-btn-primary px-10 py-2.5 rounded-xl font-black uppercase tracking-[0.2em] text-xs transition-all active:scale-95 flex items-center space-x-2 shadow-glow-sm hover:shadow-glow"
            >
              {generating ? <RefreshCw className="animate-spin" size={14} /> : <Download size={14} />}
              <span>{generating ? 'Compiling...' : 'Generate Payload'}</span>
            </button>
          </div>
        </div>
      </div>
    </Modal>
  );
};

export default GenerateAgentDialog;
