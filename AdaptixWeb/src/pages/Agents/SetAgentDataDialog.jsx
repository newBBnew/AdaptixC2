import React, { useState, useEffect } from 'react';
import Modal from '../../components/Modal';
import { cn } from '../../utils/cn';
import { agentApi } from '../../api/agent';
import { 
  Network, 
  Cpu, 
  Monitor, 
  User, 
  Shield, 
  Clock, 
  Save,
  Info
} from 'lucide-react';

const SetAgentDataDialog = ({ isOpen, onClose, agent, onUpdated }) => {
  const [formData, setFormData] = useState({
    internal_ip: '',
    external_ip: '',
    gmt_offset: 0,
    acp: 0,
    oemcp: 0,
    pid: '',
    tid: '',
    arch: 'x64',
    elevated: false,
    process: '',
    os: 1,
    os_desc: '',
    domain: '',
    computer: '',
    username: '',
    impersonated: ''
  });

  useEffect(() => {
    if (agent && isOpen) {
      setFormData({
        agent_id: agent.a_id,
        internal_ip: agent.a_internal_ip || '',
        external_ip: agent.a_external_ip || '',
        gmt_offset: agent.a_gmt_offset || 0,
        acp: agent.a_acp || 0,
        oemcp: agent.a_oemcp || 0,
        pid: agent.a_pid || '',
        tid: agent.a_tid || '',
        arch: agent.a_arch || 'x64',
        elevated: agent.a_elevated || false,
        process: agent.a_process || '',
        os: agent.a_os || 1,
        os_desc: agent.a_os_desc || '',
        domain: agent.a_domain || '',
        computer: agent.a_computer || '',
        username: agent.a_username || '',
        impersonated: agent.a_impersonated || ''
      });
    }
  }, [agent, isOpen]);

  const handleUpdate = async () => {
    try {
      await agentApi.updateData(formData);
      onUpdated?.();
      onClose();
    } catch (err) {
      console.error('Failed to update agent data:', err);
      alert('Error updating agent data');
    }
  };

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title="Set Agent Data"
      width="max-w-3xl"
    >
      <div className="flex flex-col bg-theme-glass-panel p-6 space-y-6 overflow-y-auto max-h-[80vh] custom-scrollbar">
        {/* Network Group */}
        <section className="space-y-3">
          <div className="flex items-center space-x-2 text-theme-accent">
            <Network size={16} />
            <h4 className="text-[10px] font-black uppercase tracking-[0.2em]">Network Interface</h4>
          </div>
          <div className="grid grid-cols-2 gap-6 glass-card-sm border border-theme-glass-light p-4 rounded-2xl">
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-bold text-theme-muted uppercase tracking-wider ml-1">Internal IP</label>
              <input value={formData.internal_ip} onChange={e => setFormData({...formData, internal_ip: e.target.value})} className="glass-input w-full font-mono py-2 px-3 text-theme-primary" />
            </div>
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-bold text-theme-muted uppercase tracking-wider ml-1">External IP</label>
              <input value={formData.external_ip} onChange={e => setFormData({...formData, external_ip: e.target.value})} className="glass-input w-full font-mono py-2 px-3 text-theme-primary" />
            </div>
          </div>
        </section>

        {/* Process Group */}
        <section className="space-y-3">
          <div className="flex items-center space-x-2 text-theme-accent-secondary">
            <Cpu size={16} />
            <h4 className="text-[10px] font-black uppercase tracking-[0.2em]">Process context</h4>
          </div>
          <div className="grid grid-cols-2 md:grid-cols-4 gap-6 glass-card-sm border border-theme-glass-light p-4 rounded-2xl">
            <div className="space-y-2 col-span-2 text-left">
              <label className="text-[10px] font-bold text-theme-muted uppercase tracking-wider ml-1">Image Path / Name</label>
              <input value={formData.process} onChange={e => setFormData({...formData, process: e.target.value})} className="glass-input w-full font-mono py-2 px-3 text-theme-primary" />
            </div>
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-bold text-theme-muted uppercase tracking-wider ml-1">Architecture</label>
              <select value={formData.arch} onChange={e => setFormData({...formData, arch: e.target.value})} className="glass-input w-full py-2 px-3 text-theme-primary">
                <option value="x86" className="bg-theme-glass-panel">x86</option>
                <option value="x64" className="bg-theme-glass-panel">x64</option>
              </select>
            </div>
            <div className="flex items-end pb-2">
              <label className="flex items-center space-x-2 cursor-pointer group">
                <input type="checkbox" checked={formData.elevated} onChange={e => setFormData({...formData, elevated: e.target.checked})} className="sr-only" />
                <div className={cn("w-5 h-5 border border-theme-glass-light rounded-lg flex items-center justify-center transition-all", formData.elevated ? "bg-theme-accent border-theme-accent shadow-glow-sm" : "bg-theme-glass group-hover:border-theme-accent/50")}>
                  {formData.elevated && <Shield size={14} className="text-white" />}
                </div>
                <span className="text-[11px] font-bold text-theme-secondary uppercase tracking-tight">Elevated</span>
              </label>
            </div>
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-bold text-theme-muted uppercase tracking-wider ml-1">PID</label>
              <input value={formData.pid} onChange={e => setFormData({...formData, pid: e.target.value})} className="glass-input w-full font-mono py-2 px-3 text-theme-primary" />
            </div>
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-bold text-theme-muted uppercase tracking-wider ml-1">TID</label>
              <input value={formData.tid} onChange={e => setFormData({...formData, tid: e.target.value})} className="glass-input w-full font-mono py-2 px-3 text-theme-primary" />
            </div>
          </div>
        </section>

        {/* OS Group */}
        <section className="space-y-3">
          <div className="flex items-center space-x-2 text-theme-accent">
            <Monitor size={16} />
            <h4 className="text-[10px] font-black uppercase tracking-[0.2em]">Platform Info</h4>
          </div>
          <div className="grid grid-cols-2 md:grid-cols-3 gap-6 glass-card-sm border border-theme-glass-light p-4 rounded-2xl">
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-bold text-theme-muted uppercase tracking-wider ml-1">OS Family</label>
              <select value={formData.os} onChange={e => setFormData({...formData, os: parseInt(e.target.value)})} className="glass-input w-full py-2 px-3 text-theme-primary">
                <option value={1} className="bg-theme-glass-panel">Windows</option>
                <option value={2} className="bg-theme-glass-panel">Linux</option>
                <option value={3} className="bg-theme-glass-panel">macOS</option>
                <option value={0} className="bg-theme-glass-panel">Unknown</option>
              </select>
            </div>
            <div className="space-y-2 col-span-2 text-left">
              <label className="text-[10px] font-bold text-theme-muted uppercase tracking-wider ml-1">Descriptor String</label>
              <input value={formData.os_desc} onChange={e => setFormData({...formData, os_desc: e.target.value})} className="glass-input w-full font-mono py-2 px-3 text-theme-primary" />
            </div>
          </div>
        </section>

        {/* Context Group */}
        <section className="space-y-3">
          <div className="flex items-center space-x-2 text-theme-accent-secondary">
            <User size={16} />
            <h4 className="text-[10px] font-black uppercase tracking-[0.2em]">Identity Context</h4>
          </div>
          <div className="grid grid-cols-2 gap-6 glass-card-sm border border-theme-glass-light p-4 rounded-2xl">
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-bold text-theme-muted uppercase tracking-wider ml-1">Computer</label>
              <input value={formData.computer} onChange={e => setFormData({...formData, computer: e.target.value})} className="glass-input w-full font-mono py-2 px-3 text-theme-primary" />
            </div>
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-bold text-theme-muted uppercase tracking-wider ml-1">Domain</label>
              <input value={formData.domain} onChange={e => setFormData({...formData, domain: e.target.value})} className="glass-input w-full font-mono py-2 px-3 text-theme-primary" />
            </div>
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-bold text-theme-muted uppercase tracking-wider ml-1">Username</label>
              <input value={formData.username} onChange={e => setFormData({...formData, username: e.target.value})} className="glass-input w-full font-mono py-2 px-3 text-theme-primary" />
            </div>
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-bold text-theme-muted uppercase tracking-wider ml-1">Impersonated</label>
              <input value={formData.impersonated} onChange={e => setFormData({...formData, impersonated: e.target.value})} className="glass-input w-full font-mono py-2 px-3 text-theme-primary" />
            </div>
          </div>
        </section>

        {/* Footer Actions */}
        <div className="flex items-center justify-end space-x-3 pt-6 border-t border-theme-glass-light">
          <button onClick={onClose} className="glass-btn px-6 py-2 text-xs font-bold text-theme-muted hover:text-theme-primary transition-all uppercase tracking-widest">Cancel</button>
          <button onClick={handleUpdate} className="glass-btn-primary px-8 py-2 text-xs font-black uppercase tracking-widest shadow-glow-sm hover:shadow-glow flex items-center space-x-2 text-white">
            <Save size={14} className="text-white" />
            <span>Update Session</span>
          </button>
        </div>
      </div>
    </Modal>
  );
};

export default SetAgentDataDialog;
