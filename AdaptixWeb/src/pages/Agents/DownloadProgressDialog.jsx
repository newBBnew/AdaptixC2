import React, { useState, useEffect, useRef } from 'react';
import { X, Download, Loader2 } from 'lucide-react';
import { cn } from '../../utils/cn';

const DownloadProgressDialog = ({ 
  isOpen, 
  onClose, 
  url, 
  filename,
  onComplete,
  onError
}) => {
  const [status, setStatus] = useState('preparing'); // preparing, downloading, completed, error
  const [progress, setProgress] = useState(0);
  const [received, setReceived] = useState(0);
  const [total, setTotal] = useState(0);
  const [speed, setSpeed] = useState(0);
  const [errorMessage, setErrorMessage] = useState('');
  const abortRef = useRef(null);
  const startTimeRef = useRef(null);

  useEffect(() => {
    if (!isOpen || !url) return;

    const startDownload = async () => {
      setStatus('downloading');
      setProgress(0);
      setReceived(0);
      setTotal(0);
      setSpeed(0);
      startTimeRef.current = Date.now();

      const controller = new AbortController();
      abortRef.current = controller;

      try {
        const response = await fetch(url, { signal: controller.signal });
        
        if (!response.ok) {
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const contentLength = response.headers.get('content-length');
        const totalSize = contentLength ? parseInt(contentLength, 10) : 0;
        setTotal(totalSize);

        const reader = response.body.getReader();
        const chunks = [];
        let receivedLength = 0;
        let lastUpdate = Date.now();
        let lastReceived = 0;

        while (true) {
          const { done, value } = await reader.read();
          
          if (done) break;
          
          chunks.push(value);
          receivedLength += value.length;
          setReceived(receivedLength);

          if (totalSize > 0) {
            setProgress(Math.round((receivedLength / totalSize) * 100));
          }

          // Calculate speed every 500ms
          const now = Date.now();
          if (now - lastUpdate >= 500) {
            const elapsed = (now - lastUpdate) / 1000;
            const bytesPerSecond = (receivedLength - lastReceived) / elapsed;
            setSpeed(bytesPerSecond / 1024); // KB/s
            lastUpdate = now;
            lastReceived = receivedLength;
          }
        }

        // Combine chunks and create blob
        const blob = new Blob(chunks);
        const downloadUrl = URL.createObjectURL(blob);
        
        // Trigger download
        const a = document.createElement('a');
        a.href = downloadUrl;
        a.download = filename || 'download';
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(downloadUrl);

        setStatus('completed');
        setProgress(100);
        onComplete?.();

      } catch (err) {
        if (err.name === 'AbortError') {
          setStatus('error');
          setErrorMessage('Download cancelled');
        } else {
          setStatus('error');
          setErrorMessage(err.message);
          onError?.(err);
        }
      }
    };

    startDownload();

    return () => {
      if (abortRef.current) {
        abortRef.current.abort();
      }
    };
  }, [isOpen, url, filename]);

  const handleCancel = () => {
    if (abortRef.current) {
      abortRef.current.abort();
    }
    onClose();
  };

  const formatSize = (bytes) => {
    if (bytes < 1024) return `${bytes} B`;
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
    return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
  };

  if (!isOpen) return null;

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title="File Transfer Protocol"
      width="max-w-md"
    >
      <div className="flex flex-col bg-theme-glass-panel p-6 space-y-6">
        <div className="flex items-center space-x-4 p-4 bg-theme-glass border border-theme-glass-light rounded-2xl shadow-glow-sm">
          <div className="p-3 rounded-xl bg-theme-glass-panel border border-theme-glass-light">
            <Download className="text-theme-accent animate-bounce" size={24} />
          </div>
          <div className="flex-1 min-w-0 text-left">
            <p className="text-[10px] font-black uppercase text-theme-muted tracking-widest mb-1">Transferring Artifact</p>
            <p className="text-sm font-mono font-bold text-theme-primary truncate">{filename || 'Retrieving resource...'}</p>
          </div>
        </div>

        <div className="space-y-4">
          <div className="flex justify-between items-end px-1">
            <div className="text-left">
              <p className="text-[10px] font-black uppercase text-theme-muted tracking-widest mb-0.5">Current Status</p>
              <p className="text-xs text-theme-accent font-bold uppercase tracking-wider">
                {status === 'preparing' && 'Negotiating with Teamserver...'}
                {status === 'downloading' && 'Syncing encrypted blocks...'}
                {status === 'completed' && 'Data transfer successfully verified'}
                {status === 'error' && 'Retrieval channel failure'}
              </p>
            </div>
            <div className="text-right">
              <span className="text-2xl font-mono font-black text-theme-primary tracking-tighter">{progress}%</span>
            </div>
          </div>

          <div className="h-2 w-full bg-theme-glass rounded-full overflow-hidden border border-theme-glass-light">
            <div 
              className={cn(
                "h-full transition-all duration-300 shadow-glow-sm",
                status === 'error' ? "bg-theme-danger" : "bg-gradient-to-r from-theme-accent to-theme-accent-secondary"
              )}
              style={{ width: `${progress}%` }}
            />
          </div>

          <div className="grid grid-cols-2 gap-4">
            <div className="p-3 bg-theme-glass-panel rounded-xl border border-theme-glass-light">
              <p className="text-[9px] font-black uppercase text-theme-muted tracking-widest mb-1">Payload Size</p>
              <p className="text-xs font-mono font-bold text-theme-primary">
                {formatSize(received)} / {total ? formatSize(total) : '?'}
              </p>
            </div>
            <div className="p-3 bg-theme-glass-panel rounded-xl border border-theme-glass-light">
              <p className="text-[9px] font-black uppercase text-theme-muted tracking-widest mb-1">Channel Throughput</p>
              <p className="text-xs font-mono font-bold text-theme-accent">
                {speed.toFixed(1)} KB/s
              </p>
            </div>
          </div>
        </div>

        {status === 'error' && (
          <div className="p-4 bg-theme-danger/10 border border-theme-danger/20 rounded-xl">
            <p className="text-[10px] font-bold text-theme-danger uppercase text-center">{errorMessage}</p>
          </div>
        )}

        <div className="flex justify-end pt-2">
          <button 
            onClick={status === 'downloading' ? () => abortRef.current?.abort() : onClose}
            className={cn(
              "px-8 py-2.5 rounded-xl font-black uppercase tracking-widest text-xs transition-all active:scale-95",
              status === 'downloading' 
                ? "glass-btn text-theme-danger border-theme-danger/30 hover:bg-theme-danger/10"
                : "glass-btn-primary text-white shadow-glow-sm hover:shadow-glow"
            )}
          >
            {status === 'downloading' ? 'Abort Transfer' : 'Dismiss Portal'}
          </button>
        </div>
      </div>
    </Modal>
  );
};

export default DownloadProgressDialog;
