import React, { useState, useEffect } from 'react';
import Modal from '../../components/Modal';
import { cn } from '../../utils/cn';
import { agentApi } from '../../api/agent';
import { 
  Cpu, 
  Save, 
  Download,
  Info,
  Check,
  Shield,
  Zap,
  Terminal
} from 'lucide-react';

const GenerateAgentDialog = ({ isOpen, onClose, listenerName, listenerType }) => {
  const [selectedAgent, setSelectedAgent] = useState('beacon');
  const [profileName, setProfileName] = useState('');
  const [saveAsProfile, setSaveAsProfile] = useState(true);
  const [generating, setGenerating] = useState(false);
  
  // Dynamic Agent config fields
  const [config, setConfig] = useState({
    sleep: '5',
    jitter: '10',
    arch: 'x64',
    format: 'Exe',
    proxy: '',
    allocator: 'WinAPI', // For beacon
    obfuscation: 'None', // For beacon
    user_agent: 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'
  });

  useEffect(() => {
    if (listenerName) {
      setProfileName(`${listenerName}_Agent`);
    }
  }, [listenerName]);

  const handleGenerate = async () => {
    setGenerating(true);
    try {
      const payload = {
        listener_name: listenerName,
        listener_type: listenerType,
        agent: selectedAgent,
        config: JSON.stringify(config)
      };

      const response = await agentApi.generate(payload);
      const { message, ok } = response.data;

      if (ok && message) {
        const [nameB64, contentB64] = message.split(':');
        const filename = atob(nameB64);
        const binaryString = atob(contentB64);
        const bytes = new Uint8Array(binaryString.length);
        for (let i = 0; i < binaryString.length; i++) {
          bytes[i] = binaryString.charCodeAt(i);
        }
        
        const blob = new Blob([bytes], { type: 'application/octet-stream' });
        const url = window.URL.createObjectURL(blob);
        const link = document.createElement('a');
        link.href = url;
        link.setAttribute('download', filename);
        document.body.appendChild(link);
        link.click();
        link.remove();
        window.URL.revokeObjectURL(url);
        
        onClose();
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

  const agentTypes = [
    { id: 'beacon', label: 'Beacon', icon: Zap },
    { id: 'gopher', label: 'Gopher', icon: Terminal },
  ];

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

        {/* Profile & Name */}
        <div className="p-6 space-y-6 overflow-y-auto custom-scrollbar bg-theme-glass-panel">
          {/* Agent Type Selection */}
          <div className="space-y-3 text-left">
            <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Agent Type</label>
            <div className="flex space-x-3">
              {agentTypes.map((type) => (
                <button
                  key={type.id}
                  onClick={() => setSelectedAgent(type.id)}
                  className={cn(
                    "flex-1 flex items-center justify-center space-x-3 py-3 px-4 rounded-2xl border transition-all uppercase font-black text-[10px] tracking-widest",
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

          {/* Config Grid */}
          <div className="grid grid-cols-2 gap-6">
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Sleep Delay (s)</label>
              <input 
                type="number"
                value={config.sleep}
                onChange={e => setConfig({...config, sleep: e.target.value})}
                className="glass-input w-full py-2.5 px-4 text-theme-primary font-mono"
              />
            </div>
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Jitter Factor (%)</label>
              <input 
                type="number"
                value={config.jitter}
                onChange={e => setConfig({...config, jitter: e.target.value})}
                className="glass-input w-full py-2.5 px-4 text-theme-primary font-mono"
              />
            </div>
          </div>

          <div className="grid grid-cols-2 gap-6">
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Target Platform</label>
              <select 
                value={config.arch}
                onChange={e => setConfig({...config, arch: e.target.value})}
                className="glass-input w-full py-2.5 px-4 text-theme-primary"
              >
                <option value="x64" className="bg-theme-glass-panel text-theme-primary">Windows x64</option>
                <option value="x86" className="bg-theme-glass-panel text-theme-primary">Windows x86</option>
              </select>
            </div>
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Output Format</label>
              <select 
                value={config.format}
                onChange={e => setConfig({...config, format: e.target.value})}
                className="glass-input w-full py-2.5 px-4 text-theme-primary"
              >
                <option value="Exe" className="bg-theme-glass-panel text-theme-primary">Executable (.exe)</option>
                <option value="Dll" className="bg-theme-glass-panel text-theme-primary">Dynamic Link (.dll)</option>
                <option value="Shellcode" className="bg-theme-glass-panel text-theme-primary">Raw Shellcode (.bin)</option>
              </select>
            </div>
          </div>

          {selectedAgent === 'beacon' && (
            <div className="grid grid-cols-2 gap-6">
              <div className="space-y-2 text-left">
                <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Memory Allocator</label>
                <select 
                  value={config.allocator}
                  onChange={e => setConfig({...config, allocator: e.target.value})}
                  className="glass-input w-full py-2.5 px-4 text-theme-primary"
                >
                  <option value="WinAPI" className="bg-theme-glass-panel text-theme-primary">WinAPI</option>
                  <option value="DirectSyscalls" className="bg-theme-glass-panel text-theme-primary">Direct Syscalls</option>
                  <option value="IndirectSyscalls" className="bg-theme-glass-panel text-theme-primary">Indirect Syscalls</option>
                </select>
              </div>
              <div className="space-y-2 text-left">
                <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Obfuscation</label>
                <select 
                  value={config.obfuscation}
                  onChange={e => setConfig({...config, obfuscation: e.target.value})}
                  className="glass-input w-full py-2.5 px-4 text-theme-primary"
                >
                  <option value="None" className="bg-theme-glass-panel text-theme-primary">None</option>
                  <option value="StackMask" className="bg-theme-glass-panel text-theme-primary">Stack Masking</option>
                  <option value="SleepMask" className="bg-theme-glass-panel text-theme-primary">Sleep Masking</option>
                </select>
              </div>
            </div>
          )}

          <div className="space-y-2 text-left">
            <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">User Agent</label>
            <input 
              type="text"
              value={config.user_agent}
              onChange={e => setConfig({...config, user_agent: e.target.value})}
              placeholder="Custom User-Agent string..."
              className="glass-input w-full py-2.5 px-4 text-theme-primary font-mono"
            />
          </div>

          {/* Save Profile Toggle */}
          <div className="pt-2">
            <label className="flex items-center space-x-3 cursor-pointer group">
              <input 
                type="checkbox"
                checked={saveAsProfile}
                onChange={(e) => setSaveAsProfile(e.target.checked)}
                className="sr-only"
              />
              <div className={cn(
                "w-5 h-5 border border-theme-glass-light rounded-lg flex items-center justify-center transition-all",
                saveAsProfile ? "bg-theme-accent border-theme-accent shadow-glow-sm" : "bg-theme-glass group-hover:border-theme-accent/50"
              )}>
                {saveAsProfile && <Check size={14} className="text-white" />}
              </div>
              <span className="text-[11px] font-bold text-theme-secondary uppercase tracking-widest">Register as Payload Profile</span>
            </label>
          </div>
        </div>

        {/* Footer Actions */}
        <div className="p-6 border-t border-theme-glass-light flex items-center justify-between bg-theme-glass-panel">
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
