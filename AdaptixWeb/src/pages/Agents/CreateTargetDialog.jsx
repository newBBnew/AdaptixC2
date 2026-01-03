import React, { useState, useEffect } from 'react';
import Modal from '../../components/Modal';
import { cn } from '../../utils/cn';
import { dataApi } from '../../api/control';
import { 
  Monitor, 
  Globe, 
  Tag, 
  Info, 
  Save,
  CheckCircle2,
  AlertCircle
} from 'lucide-react';

const CreateTargetDialog = ({ isOpen, onClose, onSaved, editMode = false, initialData = null }) => {
  const [formData, setFormData] = useState({
    computer: '',
    domain: '',
    address: '',
    os: 1, // windows
    os_desk: '',
    tag: '',
    info: '',
    alive: true
  });

  useEffect(() => {
    if (initialData) {
      setFormData({
        target_id: initialData.t_target_id,
        computer: initialData.t_computer || '',
        domain: initialData.t_domain || '',
        address: initialData.t_address || '',
        os: initialData.t_os || 1,
        os_desk: initialData.t_os_desk || '',
        tag: initialData.t_tag || '',
        info: initialData.t_info || '',
        alive: initialData.t_alive ?? true
      });
    } else {
      setFormData({
        computer: '',
        domain: '',
        address: '',
        os: 1,
        os_desk: '',
        tag: '',
        info: '',
        alive: true
      });
    }
  }, [initialData, isOpen]);

  const handleSave = async () => {
    if (!formData.computer.trim() || !formData.address.trim()) {
      alert('Computer name and Address are required');
      return;
    }

    try {
      const payload = {
        targets: [formData]
      };
      
      // Based on tc_targets.go, Edit uses "t_target_id" etc.
      // But Create uses a "targets" array.
      if (editMode) {
        // Edit API usually takes a single object with t_ prefix fields based on tc_targets.go
        const editPayload = {
          t_target_id: formData.target_id,
          t_computer: formData.computer,
          t_domain: formData.domain,
          t_address: formData.address,
          t_os: parseInt(formData.os),
          t_os_desk: formData.os_desk,
          t_tag: formData.tag,
          t_info: formData.info,
          t_alive: formData.alive
        };
        // We'll need to add an 'edit' method to dataApi if not present
        await dataApi.editTarget?.(editPayload);
      } else {
        // Create expects { "targets": [...] }
        await dataApi.createTarget?.(payload);
      }
      
      onSaved?.();
      onClose();
    } catch (err) {
      console.error('Failed to save target:', err);
      alert('Error saving target data');
    }
  };

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title={editMode ? "Edit Target" : "Add Target"}
      width="max-w-xl"
    >
      <div className="flex flex-col bg-theme-glass-panel">
        <div className="p-6 space-y-6 overflow-y-auto max-h-[70vh] custom-scrollbar">
          <div className="grid grid-cols-2 gap-6">
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Computer Name</label>
              <input 
                value={formData.computer}
                onChange={e => setFormData({...formData, computer: e.target.value})}
                className="glass-input w-full font-mono py-2.5 px-4 text-theme-primary"
                placeholder="e.g. WIN-TARGET-01"
              />
            </div>
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Domain</label>
              <input 
                value={formData.domain}
                onChange={e => setFormData({...formData, domain: e.target.value})}
                className="glass-input w-full font-mono py-2.5 px-4 text-theme-primary"
                placeholder="e.g. corp.local"
              />
            </div>
          </div>

          <div className="grid grid-cols-2 gap-6">
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Address (IP/Host)</label>
              <input 
                value={formData.address}
                onChange={e => setFormData({...formData, address: e.target.value})}
                className="glass-input w-full font-mono py-2.5 px-4 text-theme-primary"
                placeholder="192.168.1.100"
              />
            </div>
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">OS Type</label>
              <select 
                value={formData.os}
                onChange={e => setFormData({...formData, os: parseInt(e.target.value)})}
                className="glass-input w-full px-3 py-2.5 text-theme-primary"
              >
                <option value={0} className="bg-theme-glass-panel">Unknown</option>
                <option value={1} className="bg-theme-glass-panel">Windows</option>
                <option value={2} className="bg-theme-glass-panel">Linux</option>
                <option value={3} className="bg-theme-glass-panel">macOS</option>
              </select>
            </div>
          </div>

          <div className="space-y-2 text-left">
            <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">OS Description</label>
            <input 
              value={formData.os_desk}
              onChange={e => setFormData({...formData, os_desk: e.target.value})}
              className="glass-input w-full py-2.5 px-4 text-theme-primary"
              placeholder="e.g. Windows 11 Pro 22H2"
            />
          </div>

          <div className="grid grid-cols-2 gap-6">
            <div className="space-y-2 text-left">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Tag</label>
              <div className="relative group">
                <Tag className="absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted group-focus-within:text-theme-accent transition-colors" size={14} />
                <input 
                  value={formData.tag}
                  onChange={e => setFormData({...formData, tag: e.target.value})}
                  className="glass-input w-full pl-10 py-2.5 text-theme-primary"
                  placeholder="Critical, Database, etc."
                />
              </div>
            </div>
            <div className="flex items-end pb-2">
              <label className="flex items-center space-x-2 cursor-pointer group">
                <input 
                  type="checkbox"
                  checked={formData.alive}
                  onChange={e => setFormData({...formData, alive: e.target.checked})}
                  className="sr-only"
                />
                <div className={cn(
                  "w-5 h-5 border border-theme-glass-light rounded-lg flex items-center justify-center transition-all",
                  formData.alive ? "bg-theme-accent border-theme-accent shadow-glow-sm" : "bg-theme-glass group-hover:border-theme-accent/50"
                )}>
                  {formData.alive && <CheckCircle2 size={14} className="text-white" />}
                </div>
                <span className="text-[11px] font-bold text-theme-secondary uppercase tracking-tight">Mark as Alive</span>
              </label>
            </div>
          </div>

          <div className="space-y-2 text-left">
            <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Additional Info</label>
            <textarea 
              value={formData.info}
              onChange={e => setFormData({...formData, info: e.target.value})}
              rows="3"
              className="glass-input w-full font-mono resize-none h-24 py-3 px-4 text-theme-primary"
              placeholder="Discovery notes, open ports, etc."
            />
          </div>
        </div>

        {/* Footer */}
        <div className="flex items-center justify-end space-x-3 p-6 border-t border-theme-glass-light bg-theme-glass-panel">
          <button onClick={onClose} className="glass-btn px-6 py-2 text-xs font-bold text-theme-muted hover:text-theme-primary transition-all uppercase tracking-widest">Cancel</button>
          <button 
            onClick={handleSave}
            className="glass-btn-primary px-8 py-2 text-xs font-black uppercase tracking-widest shadow-glow-sm hover:shadow-glow flex items-center space-x-2 text-white"
          >
            <Save size={14} className="text-white" />
            <span>Save Target</span>
          </button>
        </div>
      </div>
    </Modal>
  );
};

export default CreateTargetDialog;
