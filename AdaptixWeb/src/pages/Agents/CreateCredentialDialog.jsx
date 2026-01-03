import React, { useState, useEffect } from 'react';
import Modal from '../../components/Modal';
import { cn } from '../../utils/cn';
import { dataApi } from '../../api/control';
import { 
  Key, 
  User, 
  Shield, 
  Tag, 
  Database,
  Save,
  Info,
  ChevronRight,
  Globe
} from 'lucide-react';

const CreateCredentialDialog = ({ isOpen, onClose, onSaved, editMode = false, initialData = null }) => {
  const [formData, setFormData] = useState({
    username: '',
    password: '',
    realm: '',
    type: 'password',
    tag: '',
    storage: '',
    host: ''
  });

  useEffect(() => {
    if (initialData) {
      setFormData({
        cred_id: initialData.cred_id,
        username: initialData.username || '',
        password: initialData.password || '',
        realm: initialData.realm || '',
        type: initialData.type || 'password',
        tag: initialData.tag || '',
        storage: initialData.storage || '',
        host: initialData.host || ''
      });
    } else {
      setFormData({
        username: '',
        password: '',
        realm: '',
        type: 'password',
        tag: '',
        storage: '',
        host: ''
      });
    }
  }, [initialData, isOpen]);

  const handleSave = async () => {
    if (!formData.username.trim() || !formData.password.trim()) {
      alert('Username and Password/Hash are required');
      return;
    }

    try {
      if (editMode) {
        await dataApi.editCred?.(formData);
      } else {
        await dataApi.createCred?.({ creds: [formData] });
      }
      onSaved?.();
      onClose();
    } catch (err) {
      console.error('Failed to save credential:', err);
      alert('Error saving credential data');
    }
  };

  // Aligned with Qt DialogCredential.cpp
  const credTypes = ['password', 'hash', 'rc4', 'aes128', 'aes256', 'token'];
  const storageTypes = ['browser', 'dpapi', 'database', 'sam', 'lsass', 'ntds', 'manual'];

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title={editMode ? "Edit Credential" : "Add Credential"}
      width="max-w-xl"
    >
      <div className="flex flex-col bg-theme-glass-panel">
        <div className="p-6 space-y-6 overflow-y-auto max-h-[70vh] custom-scrollbar">
          {/* Identity Section */}
          <div className="grid grid-cols-2 gap-6">
            <div className="space-y-2">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Username</label>
              <div className="relative group">
                <User className="absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted group-focus-within:text-theme-accent transition-colors" size={14} />
                <input 
                  value={formData.username}
                  onChange={e => setFormData({...formData, username: e.target.value})}
                  className="glass-input w-full pl-10 py-2.5 text-theme-primary"
                  placeholder="e.g. Administrator"
                />
              </div>
            </div>
            <div className="space-y-2">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Domain / Realm</label>
              <div className="relative group">
                <Globe className="absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted group-focus-within:text-theme-accent transition-colors" size={14} />
                <input 
                  value={formData.realm}
                  onChange={e => setFormData({...formData, realm: e.target.value})}
                  className="glass-input w-full pl-10 py-2.5 text-theme-primary"
                  placeholder="e.g. AD.LOCAL"
                />
              </div>
            </div>
          </div>

          {/* Credential Material */}
          <div className="space-y-2">
            <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Credential Material (Password / Hash)</label>
            <div className="relative group">
              <Key className="absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted group-focus-within:text-theme-accent transition-colors" size={14} />
              <input 
                value={formData.password}
                onChange={e => setFormData({...formData, password: e.target.value})}
                className="glass-input w-full pl-10 py-2.5 font-mono text-theme-primary"
                placeholder="Enter password or NTLM hash..."
              />
            </div>
          </div>

          {/* Metadata Section */}
          <div className="grid grid-cols-2 gap-6">
            <div className="space-y-2">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Material Type</label>
              <select 
                value={formData.type}
                onChange={e => setFormData({...formData, type: e.target.value})}
                className="glass-input w-full px-3 py-2.5 text-theme-primary"
              >
                {credTypes.map(t => <option key={t} value={t} className="bg-theme-glass-panel">{t.toUpperCase()}</option>)}
              </select>
            </div>
            <div className="space-y-2">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Source Tag</label>
              <div className="relative group">
                <Tag className="absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted group-focus-within:text-theme-accent transition-colors" size={14} />
                <input 
                  value={formData.tag}
                  onChange={e => setFormData({...formData, tag: e.target.value})}
                  className="glass-input w-full pl-10 py-2.5 text-theme-primary"
                  placeholder="e.g. Domain Admin"
                />
              </div>
            </div>
          </div>

          <div className="grid grid-cols-2 gap-6">
            <div className="space-y-2">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Origin Node</label>
              <div className="relative group">
                <Shield className="absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted group-focus-within:text-theme-accent transition-colors" size={14} />
                <input 
                  value={formData.host}
                  onChange={e => setFormData({...formData, host: e.target.value})}
                  className="glass-input w-full pl-10 py-2.5 text-theme-primary"
                  placeholder="e.g. DC01"
                />
              </div>
            </div>
            <div className="space-y-2">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Storage Provenance</label>
              <select 
                value={formData.storage}
                onChange={e => setFormData({...formData, storage: e.target.value})}
                className="glass-input w-full px-3 py-2.5 text-theme-primary"
              >
                <option value="" className="bg-theme-glass-panel">UNSPECIFIED</option>
                {storageTypes.map(t => <option key={t} value={t} className="bg-theme-glass-panel">{t.toUpperCase()}</option>)}
              </select>
            </div>
          </div>
        </div>

        {/* Footer Actions */}
        <div className="flex items-center justify-end space-x-3 p-6 border-t border-theme-glass-light bg-theme-glass-panel">
          <button onClick={onClose} className="glass-btn px-6 py-2 text-xs font-bold text-theme-muted hover:text-theme-primary transition-all uppercase tracking-widest">Cancel</button>
          <button 
            onClick={handleSave}
            className="glass-btn-primary px-8 py-2 text-xs font-black uppercase tracking-widest shadow-glow-sm hover:shadow-glow flex items-center space-x-2 text-white"
          >
            <Save size={14} className="text-white" />
            <span>Save Loot</span>
          </button>
        </div>
      </div>
    </Modal>
  );
};

export default CreateCredentialDialog;
