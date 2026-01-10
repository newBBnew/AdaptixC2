import React, { useState } from 'react';
import Modal from '../../components/Modal';
import { cn } from '../../utils/cn';
import { deliveryApi } from '../../api/control';
import { 
  Link as LinkIcon, 
  Clock, 
  Shield, 
  Copy,
  Save,
  Info
} from 'lucide-react';

const CreateLinkDialog = ({ isOpen, onClose, fileId, filename }) => {
  const [hours, setHours] = useState(24);
  const [createdUrl, setCreatedUrl] = useState('');
  const [loading, setLoading] = useState(false);

  const handleCreate = async () => {
    setLoading(true);
    try {
      // payload matches TcFileDeliveryCreateLink struct in tc_filedelivery.go
      const response = await deliveryApi.createLink({
        file_id: fileId,
        expire_hours: parseInt(hours),
        max_uses: 0, // 0 for unlimited
        allowed_ip: ''
      });
      
      if (response.data?.ok) {
        const token = response.data.token;
        const baseUrl = localStorage.getItem('adaptix_url') || window.location.origin;
        setCreatedUrl(`${baseUrl}/download/${token}`);
      } else {
        alert(response.data?.message || 'Failed to create link');
      }
    } catch (err) {
      console.error('Link creation failed:', err);
      alert('Error creating download link');
    } finally {
      setLoading(false);
    }
  };

  const copyToClipboard = () => {
    navigator.clipboard.writeText(createdUrl);
    alert('URL copied to clipboard');
  };

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title="Create Download Link"
      width="max-w-md"
    >
      <div className="flex flex-col bg-theme-glass-panel p-6 space-y-6">
        {!createdUrl ? (
          <div className="space-y-6">
            <div className="flex items-center space-x-4 p-4 bg-theme-glass border border-theme-glass-light rounded-2xl shadow-glow-sm">
              <LinkIcon className="text-theme-accent opacity-80" size={20} />
              <div className="overflow-hidden text-left">
                <p className="text-[10px] font-black uppercase text-theme-muted tracking-widest">Target Resource</p>
                <p className="text-sm font-mono font-bold text-theme-primary truncate">{filename}</p>
              </div>
            </div>

            <div className="space-y-3">
              <label className="text-[10px] font-black uppercase text-theme-muted tracking-widest flex items-center justify-between px-1">
                <span>Expiration</span>
                <span className="text-theme-accent font-mono font-bold">{hours} HOURS</span>
              </label>
              <div className="px-2">
                <input 
                  type="range"
                  min="1"
                  max="168" 
                  value={hours}
                  onChange={(e) => setHours(e.target.value)}
                  className="w-full h-1.5 bg-theme-glass rounded-full appearance-none cursor-pointer accent-theme-accent"
                />
              </div>
              <div className="flex justify-between text-[9px] text-theme-muted font-bold uppercase px-1">
                <span>1H</span>
                <span>24H</span>
                <span>168H (WEEK)</span>
              </div>
            </div>

            <div className="flex items-start space-x-3 p-4 bg-theme-glass-panel border border-theme-glass-light rounded-2xl">
              <Info size={16} className="text-theme-accent-secondary shrink-0 mt-0.5" />
              <p className="text-[10px] text-theme-muted leading-relaxed uppercase font-bold tracking-tight text-left">
                Generates a unique one-time token. Anyone with the link can download the file until expiration.
              </p>
            </div>

            <div className="flex justify-end pt-2">
              <button
                onClick={handleCreate}
                disabled={loading}
                className="glass-btn-primary px-10 py-3 rounded-xl font-black uppercase tracking-[0.2em] text-xs transition-all active:scale-95 flex items-center space-x-2 text-white"
              >
                {loading ? <Clock className="animate-spin" size={14} /> : <Save size={14} className="text-white" />}
                <span>{loading ? 'Creating...' : 'Generate Link'}</span>
              </button>
            </div>
          </div>
        ) : (
          <div className="space-y-6 animate-in zoom-in duration-300">
            <div className="p-6 bg-theme-glass border border-theme-glass-light rounded-2xl text-center space-y-4 shadow-glow-sm">
              <div className="w-16 h-16 bg-theme-glass rounded-full border border-theme-glass-light flex items-center justify-center mx-auto">
                <LinkIcon size={32} className="text-theme-accent" />
              </div>
              <div>
                <h3 className="text-lg font-black text-theme-primary tracking-tight uppercase">Transfer URL Ready</h3>
                <p className="text-xs text-theme-muted font-bold uppercase mt-1">Encrypted retrieval channel established</p>
              </div>
            </div>

            <div className="relative group">
              <input 
                type="text"
                readOnly
                value={createdUrl}
                className="w-full glass-input pl-4 pr-12 py-4 font-mono text-xs text-theme-accent select-all rounded-2xl"
              />
              <button 
                onClick={copyToClipboard}
                className="absolute right-3 top-1/2 -translate-y-1/2 p-2 glass-btn text-theme-muted hover:text-theme-accent transition-all rounded-xl"
              >
                <Copy size={18} />
              </button>
            </div>

            <button 
              onClick={onClose}
              className="w-full glass-btn py-4 rounded-2xl font-black uppercase tracking-[0.2em] text-xs text-theme-primary hover:bg-theme-hover transition-all"
            >
              Close Portal
            </button>
          </div>
        )}
      </div>
    </Modal>
  );
};

export default CreateLinkDialog;

import { Check } from 'lucide-react';
