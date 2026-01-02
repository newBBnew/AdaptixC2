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
      <div className="flex flex-col bg-dark-900/50">
        <div className="p-6 space-y-6 overflow-y-auto">
          {/* Identity Section */}
          <div className="grid grid-cols-2 gap-4">
            <div className="space-y-2">
              <label className="text-[10px] font-black uppercase text-gray-500 tracking-widest">Username</label>
              <div className="relative">
                <User className="absolute left-3 top-1/2 -translate-y-1/2 text-gray-600" size={14} />
                <input 
                  value={formData.username}
                  onChange={e => setFormData({...formData, username: e.target.value})}
                  className="w-full bg-dark-950 border border-dark-600 rounded pl-9 pr-3 py-2 text-sm text-white outline-none focus:border-accent-primary"
                  placeholder="e.g. Administrator"
                />
              </div>
            </div>
            <div className="space-y-2">
              <label className="text-[10px] font-black uppercase text-gray-500 tracking-widest">Realm / Domain</label>
              <div className="relative">
                <Globe className="absolute left-3 top-1/2 -translate-y-1/2 text-gray-600" size={14} />
                <input 
                  value={formData.realm}
                  onChange={e => setFormData({...formData, realm: e.target.value})}
                  className="w-full bg-dark-950 border border-dark-600 rounded pl-9 pr-3 py-2 text-sm text-white outline-none focus:border-accent-primary"
                  placeholder="e.g. CORP.LOCAL"
                />
              </div>
            </div>
          </div>

          {/* Secret Section */}
          <div className="space-y-2">
            <label className="text-[10px] font-black uppercase text-gray-500 tracking-widest">Password / Hash / Key</label>
            <textarea 
              value={formData.password}
              onChange={e => setFormData({...formData, password: e.target.value})}
              rows="2"
              className="w-full bg-dark-950 border border-dark-600 rounded px-3 py-2 text-sm text-gray-300 font-mono focus:border-accent-primary outline-none resize-none"
              placeholder="Enter cleartext password or hash..."
            />
          </div>

          {/* Type & Tag */}
          <div className="grid grid-cols-2 gap-4">
            <div className="space-y-2">
              <label className="text-[10px] font-black uppercase text-gray-500 tracking-widest">Credential Type</label>
              <select 
                value={formData.type}
                onChange={e => setFormData({...formData, type: e.target.value})}
                className="w-full bg-dark-950 border border-dark-600 rounded px-3 py-2 text-sm text-white outline-none focus:border-accent-primary"
              >
                {credTypes.map(t => <option key={t} value={t}>{t.toUpperCase()}</option>)}
              </select>
            </div>
            <div className="space-y-2">
              <label className="text-[10px] font-black uppercase text-gray-500 tracking-widest">Tag</label>
              <div className="relative">
                <Tag className="absolute left-3 top-1/2 -translate-y-1/2 text-gray-600" size={14} />
                <input 
                  value={formData.tag}
                  onChange={e => setFormData({...formData, tag: e.target.value})}
                  className="w-full bg-dark-950 border border-dark-600 rounded pl-9 pr-3 py-2 text-sm text-white outline-none focus:border-accent-primary"
                  placeholder="e.g. Domain Admin"
                />
              </div>
            </div>
          </div>

          {/* Metadata */}
          <div className="grid grid-cols-2 gap-4">
            <div className="space-y-2">
              <label className="text-[10px] font-black uppercase text-gray-500 tracking-widest">Origin Host</label>
              <input 
                value={formData.host}
                onChange={e => setFormData({...formData, host: e.target.value})}
                className="w-full bg-dark-950 border border-dark-600 rounded px-3 py-2 text-sm text-white outline-none focus:border-accent-primary font-mono"
                placeholder="Source IP/Hostname"
              />
            </div>
            <div className="space-y-2">
              <label className="text-[10px] font-black uppercase text-gray-500 tracking-widest">Storage Note</label>
              <input 
                value={formData.storage}
                onChange={e => setFormData({...formData, storage: e.target.value})}
                className="w-full bg-dark-950 border border-dark-600 rounded px-3 py-2 text-sm text-white outline-none focus:border-accent-primary"
                placeholder="SAM, LSASS, etc."
              />
            </div>
          </div>
        </div>

        {/* Footer */}
        <div className="p-4 bg-dark-800 border-t border-dark-700 flex items-center justify-end space-x-3 px-6">
          <button onClick={onClose} className="px-4 py-2 rounded text-xs font-bold text-gray-400 hover:bg-dark-700 uppercase tracking-widest transition-colors">Cancel</button>
          <button 
            onClick={handleSave}
            className="flex items-center space-x-2 px-6 py-2 bg-accent-primary hover:bg-accent-primary/90 text-white rounded text-xs font-black uppercase tracking-widest transition-all shadow-lg shadow-accent-primary/20"
          >
            <Save size={14} />
            <span>Loot Credential</span>
          </button>
        </div>
      </div>
    </Modal>
  );
};

export default CreateCredentialDialog;
