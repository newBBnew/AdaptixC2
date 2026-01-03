import React, { useState, useRef } from 'react';
import Modal from '../../components/Modal';
import { cn } from '../../utils/cn';
import { deliveryApi } from '../../api/control';
import { 
  FileUp, 
  X, 
  Upload, 
  CheckCircle2, 
  AlertCircle,
  Loader2,
  FileText
} from 'lucide-react';

const UploadFileDialog = ({ isOpen, onClose, onUploaded }) => {
  const [file, setFile] = useState(null);
  const [uploading, setUploading] = useState(false);
  const [progress, setProgress] = useState(0);
  const fileInputRef = useRef(null);

  const handleFileChange = (e) => {
    const selectedFile = e.target.files[0];
    if (selectedFile) {
      setFile(selectedFile);
    }
  };

  const handleUpload = async () => {
    if (!file) return;

    setUploading(true);
    setProgress(0);

    const formData = new FormData();
    formData.append('file', file);

    try {
      const response = await deliveryApi.upload(formData);
      if (response.data?.ok) {
        onUploaded?.();
        onClose();
      } else {
        alert(response.data?.message || 'Upload failed');
      }
    } catch (err) {
      console.error('Upload failed:', err);
      alert('Error uploading file to Teamserver');
    } finally {
      setUploading(false);
      setFile(null);
    }
  };

  const dropZoneRef = useRef(null);

  const handleDragOver = (e) => {
    e.preventDefault();
    e.stopPropagation();
  };

  const handleDrop = (e) => {
    e.preventDefault();
    e.stopPropagation();
    const droppedFile = e.dataTransfer.files[0];
    if (droppedFile) {
      setFile(droppedFile);
    }
  };

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title="Upload File to Host"
      width="max-w-md"
    >
      <div className="flex flex-col bg-theme-glass-panel p-6 space-y-6">
        <div 
          ref={dropZoneRef}
          onDragOver={handleDragOver}
          onDrop={handleDrop}
          onClick={() => fileInputRef.current?.click()}
          className={cn(
            "border-2 border-dashed rounded-3xl p-10 flex flex-col items-center justify-center space-y-4 cursor-pointer transition-all duration-300",
            file 
              ? "border-theme-accent bg-theme-accent/5 shadow-glow-sm" 
              : "border-theme-glass-light hover:border-theme-accent/50 hover:bg-theme-glass"
          )}
        >
          <input 
            type="file" 
            ref={fileInputRef} 
            onChange={handleFileChange} 
            className="hidden" 
          />
          
          {file ? (
            <div className="animate-in zoom-in duration-300 text-center space-y-3">
              <div className="w-16 h-16 bg-theme-accent/10 rounded-2xl flex items-center justify-center text-theme-accent mx-auto border border-theme-accent/20">
                <FileText size={32} />
              </div>
              <div>
                <p className="text-sm font-bold text-theme-primary tracking-tight">{file.name}</p>
                <p className="text-[10px] text-theme-muted font-black uppercase tracking-widest mt-1">
                  {(file.size / 1024).toFixed(1)} KB • Ready for uplink
                </p>
              </div>
            </div>
          ) : (
            <div className="text-center space-y-3">
              <div className="w-16 h-16 bg-theme-glass rounded-2xl flex items-center justify-center text-theme-muted mx-auto border border-theme-glass-light group-hover:text-theme-accent transition-colors">
                <FileUp size={32} />
              </div>
              <div>
                <p className="text-sm font-bold text-theme-primary">Drop artifact or click to browse</p>
                <p className="text-[10px] text-theme-muted font-black uppercase tracking-widest mt-1">Maximum payload: 500MB</p>
              </div>
            </div>
          )}
        </div>

        {uploading && (
          <div className="space-y-2 animate-in fade-in duration-300">
            <div className="flex justify-between text-[10px] font-black uppercase text-theme-muted tracking-widest px-1">
              <span>Uplinking to Teamserver</span>
              <span className="text-theme-accent">In progress</span>
            </div>
            <div className="h-1.5 w-full bg-theme-glass rounded-full overflow-hidden border border-theme-glass-light">
              <div className="h-full bg-theme-accent animate-pulse shadow-glow-sm" style={{ width: '100%' }} />
            </div>
          </div>
        )}

        <div className="flex items-center justify-between pt-2">
          <div className="flex items-center space-x-2 text-theme-muted italic text-[10px] uppercase font-bold tracking-tighter">
            <Info size={14} className="text-theme-accent" />
            <span>Secure TLS 1.3 Transmission</span>
          </div>
          <div className="flex items-center space-x-3">
            <button 
              onClick={onClose}
              className="glass-btn px-6 py-2 text-xs font-bold text-theme-muted hover:text-theme-primary transition-all uppercase tracking-widest"
            >
              Cancel
            </button>
            <button 
              onClick={handleUpload}
              disabled={!file || uploading}
              className="glass-btn-primary px-8 py-2 text-xs font-black uppercase tracking-widest shadow-glow-sm hover:shadow-glow flex items-center space-x-2 disabled:opacity-50 disabled:cursor-not-allowed text-white"
            >
              {uploading ? <Loader2 className="animate-spin text-white" size={14} /> : <Upload size={14} className="text-white" />}
              <span>{uploading ? 'Transferring...' : 'Start Upload'}</span>
            </button>
          </div>
        </div>
      </div>
    </Modal>
  );
};

export default UploadFileDialog;
