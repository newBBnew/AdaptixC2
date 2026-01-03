import React, { useState, useEffect } from 'react';
import Modal from '../../components/Modal';
import { cn } from '../../utils/cn';
import { listenerApi } from '../../api/control';
import { 
  Globe, 
  Shield, 
  Settings, 
  Save, 
  Plus,
  ChevronRight,
  Info,
  Check
} from 'lucide-react';

const CreateListenerDialog = ({ isOpen, onClose, onCreated, editMode = false, initialData = null }) => {
  const [activeTab, setActiveTab] = useState('config'); // 'config' or 'profiles'
  const [listenerName, setListenerName] = useState('');
  const [selectedType, setSelectedType] = useState('BeaconHTTP');
  const [profileName, setProfileName] = useState('');
  const [saveAsProfile, setSaveAsProfile] = useState(true);
  
  // Dynamic config fields based on type
  const [config, setConfig] = useState({
    bind_host: '0.0.0.0',
    bind_port: '80',
    agent_host: '',
    proxy: '',
    user_agent: 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
    header: '',
    pipe_name: 'adaptix_pipe', // For SMB
    bind_port_tcp: '4444', // For TCP
    domain: '', // For DoH
    resolver: '8.8.8.8' // For DoH
  });

  useEffect(() => {
    if (initialData) {
      setListenerName(initialData.l_name || '');
      setSelectedType(initialData.l_type || 'BeaconHTTP');
      try {
        const parsedConfig = typeof initialData.l_config === 'string' 
          ? JSON.parse(initialData.l_config) 
          : initialData.l_config;
        if (parsedConfig) setConfig(prev => ({ ...prev, ...parsedConfig }));
      } catch (e) {
        console.error('Failed to parse listener config', e);
      }
    } else {
      setListenerName('');
      setProfileName('');
    }
  }, [initialData]);

  const handleCreate = async () => {
    if (!listenerName.trim()) {
      alert('Listener name is required');
      return;
    }

    const payload = {
      name: listenerName,
      type: selectedType,
      config: JSON.stringify(config)
    };

    try {
      if (editMode) {
        await listenerApi.edit(payload);
      } else {
        await listenerApi.start(payload);
      }
      onCreated?.();
      onClose();
    } catch (err) {
      console.error('Failed to save listener:', err);
      alert(err.response?.data?.message || 'Failed to save listener');
    }
  };

  const listenerTypes = [
    { id: 'BeaconHTTP', label: 'HTTP Beacon', icon: Globe, protocol: 'http' },
    { id: 'BeaconDoH', label: 'DoH Beacon', icon: Shield, protocol: 'doh' },
    { id: 'BeaconTCP', label: 'TCP Beacon', icon: Settings, protocol: 'bind_tcp' },
    { id: 'BeaconSMB', label: 'SMB Beacon', icon: Settings, protocol: 'bind_smb' },
  ];

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title={editMode ? "Edit Listener" : "Create Listener"}
      width="max-w-2xl"
    >
      <div className="flex flex-col h-full bg-theme-glass-panel">
        {/* Top Header Section */}
        <div className="p-6 grid grid-cols-2 gap-6 bg-theme-glass-panel border-b border-theme-glass-light">
          <div className="space-y-2 text-left">
            <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Listener Name</label>
            <input 
              type="text"
              value={listenerName}
              onChange={(e) => {
                setListenerName(e.target.value);
                if (saveAsProfile) setProfileName(e.target.value);
              }}
              placeholder="e.g. HTTP_External"
              disabled={editMode}
              className="glass-input w-full font-mono py-2.5 px-4 text-theme-primary"
            />
          </div>
          <div className="space-y-2 relative text-left">
            <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Save as Profile</label>
            <div className="relative group">
              <input 
                type="text"
                value={profileName}
                onChange={(e) => setProfileName(e.target.value)}
                placeholder="Profile identifier..."
                disabled={!saveAsProfile || editMode}
                className="glass-input w-full font-mono italic pr-10 py-2.5 px-4 text-theme-primary"
              />
              <button 
                onClick={() => setSaveAsProfile(!saveAsProfile)}
                className={cn(
                  "absolute right-3 top-1/2 -translate-y-1/2 p-1.5 rounded-lg transition-colors",
                  saveAsProfile ? "text-theme-accent bg-theme-glass" : "text-theme-muted"
                )}
              >
                <Check size={16} />
              </button>
            </div>
          </div>
        </div>

        {/* Type Selection */}
        <div className="px-6 py-4 bg-theme-glass-panel border-b border-theme-glass-light text-left">
          <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest block mb-3 ml-1">Protocol Type</label>
          <div className="flex space-x-3">
            {listenerTypes.map((type) => (
              <button
                key={type.id}
                disabled={editMode}
                onClick={() => setSelectedType(type.id)}
                className={cn(
                  "flex-1 flex items-center justify-center space-x-2 py-2.5 px-4 rounded-xl border transition-all uppercase font-black text-[10px] tracking-widest",
                  selectedType === type.id 
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

        {/* Dynamic Config Form */}
        <div className="p-6 space-y-6 overflow-y-auto custom-scrollbar bg-theme-glass-panel">
          {/* Common Fields */}
          <div className="grid grid-cols-2 gap-6">
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Bind Host</label>
              <input 
                type="text"
                value={config.bind_host}
                onChange={(e) => setConfig({...config, bind_host: e.target.value})}
                className="glass-input w-full font-mono py-2.5 px-4 text-theme-primary"
              />
            </div>
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Bind Port</label>
              <input 
                type="text"
                value={config.bind_port}
                onChange={(e) => setConfig({...config, bind_port: e.target.value})}
                className="glass-input w-full font-mono text-center py-2.5 px-4 text-theme-primary"
              />
            </div>
          </div>

          {/* Type Specific Fields */}
          {selectedType === 'BeaconHTTP' && (
            <>
              <div className="space-y-2 text-left">
                <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">User Agent</label>
                <input 
                  type="text"
                  value={config.user_agent}
                  onChange={(e) => setConfig({...config, user_agent: e.target.value})}
                  className="glass-input w-full font-mono py-2.5 px-4 text-theme-primary"
                />
              </div>
              <div className="space-y-2 text-left">
                <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest flex items-center ml-1">
                  Agent Host Address(es)
                  <Info size={12} className="ml-2 text-theme-muted" />
                </label>
                <textarea 
                  rows="2"
                  value={config.agent_host}
                  onChange={(e) => setConfig({...config, agent_host: e.target.value})}
                  placeholder="Primary host address for agents..."
                  className="glass-input w-full font-mono resize-none h-24 py-3 px-4 text-theme-primary"
                />
              </div>
            </>
          )}

          {selectedType === 'BeaconDoH' && (
            <>
              <div className="space-y-2 text-left">
                <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Domain</label>
                <input 
                  type="text"
                  value={config.domain}
                  onChange={(e) => setConfig({...config, domain: e.target.value})}
                  placeholder="doh.example.com"
                  className="glass-input w-full font-mono py-2.5 px-4 text-theme-primary"
                />
              </div>
              <div className="space-y-2 text-left">
                <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Resolver Address</label>
                <input 
                  type="text"
                  value={config.resolver}
                  onChange={(e) => setConfig({...config, resolver: e.target.value})}
                  placeholder="8.8.8.8"
                  className="glass-input w-full font-mono py-2.5 px-4 text-theme-primary"
                />
              </div>
            </>
          )}

          {selectedType === 'BeaconSMB' && (
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Pipe Name</label>
              <input 
                type="text"
                value={config.pipe_name}
                onChange={(e) => setConfig({...config, pipe_name: e.target.value})}
                placeholder="adaptix_pipe"
                className="glass-input w-full font-mono py-2.5 px-4 text-theme-primary"
              />
            </div>
          )}

          {selectedType === 'BeaconTCP' && (
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Local Port</label>
              <input 
                type="text"
                value={config.bind_port_tcp}
                onChange={(e) => setConfig({...config, bind_port_tcp: e.target.value})}
                placeholder="4444"
                className="glass-input w-full font-mono py-2.5 px-4 text-theme-primary"
              />
            </div>
          )}
        </div>

        {/* Footer Actions */}
        <div className="p-6 border-t border-theme-glass-light flex items-center justify-between bg-theme-glass-panel">
          <div className="flex items-center space-x-2 text-theme-muted italic text-[10px] uppercase font-bold tracking-tighter">
            <Info size={14} className="text-theme-accent" />
            <span>Server validation required</span>
          </div>
          <div className="flex items-center space-x-3">
            <button 
              onClick={onClose}
              className="glass-btn px-6 py-2 text-xs font-bold text-theme-muted hover:text-theme-primary transition-all uppercase tracking-widest"
            >
              Cancel
            </button>
            <button 
              onClick={handleCreate}
              className="glass-btn-primary px-8 py-2 text-xs font-black uppercase tracking-widest shadow-glow-sm hover:shadow-glow flex items-center space-x-2 text-white"
            >
              <Save size={14} className="text-white" />
              <span>{editMode ? 'Update' : 'Launch'}</span>
            </button>
          </div>
        </div>
      </div>
    </Modal>
  );
};

export default CreateListenerDialog;
