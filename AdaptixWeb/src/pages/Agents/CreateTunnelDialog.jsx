import React, { useState, useEffect } from 'react';
import Modal from '../../components/Modal';
import { cn } from '../../utils/cn';
import { tunnelApi } from '../../api/control';
import { 
  Shield, 
  Globe, 
  ArrowRight,
  Info,
  Save,
  Network
} from 'lucide-react';

const CreateTunnelDialog = ({ isOpen, onClose, agentId, onCreated }) => {
  const [type, setType] = useState('socks5'); // socks5, socks4, lportfwd, rportfwd
  const [endpoint, setEndpoint] = useState('Teamserver');
  const [description, setDescription] = useState('');
  
  // Dynamic settings based on type
  const [settings, setSettings] = useState({
    l_host: '0.0.0.0',
    l_port: '1080',
    t_host: '127.0.0.1',
    t_port: '8000',
    use_auth: false,
    username: '',
    password: '',
  });

  const tunnelTypes = [
    { id: 'socks5', label: 'Socks5 Proxy' },
    { id: 'socks4', label: 'Socks4 Proxy' },
    { id: 'lportfwd', label: 'Local Forward' },
    { id: 'rportfwd', label: 'Reverse Forward' },
  ];

  const handleCreate = async () => {
    try {
      let response;
      const commonData = {
        agent_id: agentId,
        listen: endpoint === 'Teamserver',
        desc: description,
      };

      if (type === 'socks5') {
        response = await tunnelApi.startSocks5({
          ...commonData,
          l_host: settings.l_host,
          l_port: parseInt(settings.l_port),
          use_auth: settings.use_auth,
          username: settings.username,
          password: settings.password
        });
      } else if (type === 'socks4') {
        response = await tunnelApi.startSocks4({
          ...commonData,
          l_host: settings.l_host,
          l_port: parseInt(settings.l_port)
        });
      } else if (type === 'lportfwd') {
        response = await tunnelApi.startLportfwd({
          ...commonData,
          l_host: settings.l_host,
          l_port: parseInt(settings.l_port),
          t_host: settings.t_host,
          t_port: parseInt(settings.t_port)
        });
      } else if (type === 'rportfwd') {
        response = await tunnelApi.startRportfwd({
          ...commonData,
          port: parseInt(settings.l_port), // Reverse forward uses 'port' field for remote port in backend
          t_host: settings.t_host,
          t_port: parseInt(settings.t_port)
        });
      }

      if (response?.data?.ok) {
        onCreated?.();
        onClose();
      } else {
        alert(response?.data?.message || 'Failed to create tunnel');
      }
    } catch (err) {
      console.error('Failed to create tunnel:', err);
      alert(err.response?.data?.message || 'Error creating tunnel');
    }
  };

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title="Create Tunnel"
      width="max-w-xl"
    >
      <div className="flex flex-col bg-theme-glass-panel">
        {/* Basic Info */}
        <div className="p-6 space-y-4 border-b border-theme-glass-light bg-theme-glass-panel">
          <div className="grid grid-cols-2 gap-6">
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Tunnel Type</label>
              <select 
                value={type}
                onChange={(e) => setType(e.target.value)}
                className="glass-input w-full py-2.5 px-4 text-theme-primary"
              >
                {tunnelTypes.map(t => <option key={t.id} value={t.id} className="bg-theme-glass-panel">{t.label}</option>)}
              </select>
            </div>
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Endpoint</label>
              <select 
                value={endpoint}
                onChange={(e) => setEndpoint(e.target.value)}
                className="glass-input w-full py-2.5 px-4 text-theme-primary"
              >
                <option className="bg-theme-glass-panel">Teamserver</option>
                <option disabled={type === 'rportfwd'} className="bg-theme-glass-panel">Client</option>
              </select>
            </div>
          </div>
          <div className="space-y-2 text-left">
            <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Description</label>
            <input 
              type="text"
              value={description}
              onChange={(e) => setDescription(e.target.value)}
              placeholder="Internal network access..."
              className="glass-input w-full font-mono py-2.5 px-4 text-theme-primary"
            />
          </div>
        </div>

        {/* Dynamic Settings */}
        <div className="p-6 space-y-6 min-h-[200px] bg-theme-glass-panel overflow-y-auto custom-scrollbar">
          {(type === 'socks5' || type === 'socks4') && (
            <div className="grid grid-cols-2 gap-6 animate-in fade-in duration-300">
              <div className="space-y-2 text-left">
                <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Listen Address</label>
                <input 
                  type="text"
                  value={settings.l_host}
                  onChange={(e) => setSettings({...settings, l_host: e.target.value})}
                  className="glass-input w-full font-mono py-2.5 px-4 text-theme-primary"
                />
              </div>
              <div className="space-y-2 text-left">
                <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Listen Port</label>
                <input 
                  type="text"
                  value={settings.l_port}
                  onChange={(e) => setSettings({...settings, l_port: e.target.value})}
                  className="glass-input w-full font-mono text-center py-2.5 px-4 text-theme-primary"
                />
              </div>
              {type === 'socks5' && (
                <div className="col-span-2 space-y-4 mt-2">
                  <label className="flex items-center space-x-3 cursor-pointer group">
                    <input 
                      type="checkbox"
                      checked={settings.use_auth}
                      onChange={(e) => setSettings({...settings, use_auth: e.target.checked})}
                      className="sr-only"
                    />
                    <div className={cn(
                      "w-5 h-5 border border-theme-glass-light rounded-lg flex items-center justify-center transition-all",
                      settings.use_auth ? "bg-theme-accent border-theme-accent shadow-glow-sm" : "bg-theme-glass group-hover:border-theme-accent/50"
                    )}>
                      {settings.use_auth && <Check size={14} className="text-white" />}
                    </div>
                    <span className="text-[11px] font-bold text-theme-secondary uppercase tracking-tight">Use Proxy Authentication</span>
                  </label>
                  {settings.use_auth && (
                    <div className="grid grid-cols-2 gap-4 animate-in slide-in-from-top-2 duration-200">
                      <input 
                        placeholder="Username"
                        value={settings.username}
                        onChange={(e) => setSettings({...settings, username: e.target.value})}
                        className="glass-input w-full py-2.5 px-4 text-theme-primary"
                      />
                      <input 
                        placeholder="Password"
                        type="password"
                        value={settings.password}
                        onChange={(e) => setSettings({...settings, password: e.target.value})}
                        className="glass-input w-full py-2.5 px-4 text-theme-primary"
                      />
                    </div>
                  )}
                </div>
              )}
            </div>
          )}

          {type === 'lportfwd' && (
            <div className="space-y-4 animate-in fade-in duration-300">
              <div className="grid grid-cols-2 gap-6">
                <div className="space-y-2 text-left">
                  <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Local Bind</label>
                  <div className="flex space-x-2">
                    <input value={settings.l_host} onChange={e => setSettings({...settings, l_host: e.target.value})} className="glass-input flex-1 font-mono py-2.5 px-4 text-theme-primary"/>
                    <input value={settings.l_port} onChange={e => setSettings({...settings, l_port: e.target.value})} className="glass-input w-24 font-mono text-center py-2.5 px-4 text-theme-primary"/>
                  </div>
                </div>
                <div className="space-y-2 text-left">
                  <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Remote Target</label>
                  <div className="flex space-x-2">
                    <input value={settings.t_host} onChange={e => setSettings({...settings, t_host: e.target.value})} className="glass-input flex-1 font-mono py-2.5 px-4 text-theme-primary"/>
                    <input value={settings.t_port} onChange={e => setSettings({...settings, t_port: e.target.value})} className="glass-input w-24 font-mono text-center py-2.5 px-4 text-theme-primary"/>
                  </div>
                </div>
              </div>
            </div>
          )}

          {type === 'rportfwd' && (
            <div className="space-y-4 animate-in fade-in duration-300">
              <div className="grid grid-cols-2 gap-6">
                <div className="space-y-2 text-left">
                  <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Remote Port</label>
                  <input value={settings.l_port} onChange={e => setSettings({...settings, l_port: e.target.value})} className="glass-input w-full font-mono text-center py-2.5 px-4 text-theme-primary"/>
                </div>
                <div className="space-y-2 text-left">
                  <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Local Target</label>
                  <div className="flex space-x-2">
                    <input value={settings.t_host} onChange={e => setSettings({...settings, t_host: e.target.value})} className="glass-input flex-1 font-mono py-2.5 px-4 text-theme-primary"/>
                    <input value={settings.t_port} onChange={e => setSettings({...settings, t_port: e.target.value})} className="glass-input w-24 font-mono text-center py-2.5 px-4 text-theme-primary"/>
                  </div>
                </div>
              </div>
            </div>
          )}
        </div>

        {/* Footer */}
        <div className="p-6 border-t border-theme-glass-light flex items-center justify-between bg-theme-glass-panel">
          <div className="flex items-center space-x-2 text-theme-muted italic text-[10px] uppercase font-bold tracking-tighter">
            <Network size={14} className="text-theme-accent" />
            <span>Traffic encryption: via agent channel</span>
          </div>
          <div className="flex items-center space-x-3">
            <button onClick={onClose} className="glass-btn px-6 py-2 text-xs font-bold text-theme-muted hover:text-theme-primary transition-all uppercase tracking-widest">Cancel</button>
            <button 
              onClick={handleCreate}
              className="glass-btn-primary px-8 py-2 text-xs font-black uppercase tracking-widest shadow-glow-sm hover:shadow-glow flex items-center space-x-2 text-white"
            >
              <Save size={14} className="text-white" />
              <span>Deploy</span>
            </button>
          </div>
        </div>
      </div>
    </Modal>
  );
};

export default CreateTunnelDialog;
