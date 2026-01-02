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
    <div className="fixed inset-0 bg-black/60 flex items-center justify-center z-50">
      <div className="bg-dark-800 border border-dark-600 rounded-lg shadow-xl w-full max-w-md">
        {/* Header */}
        <div className="flex items-center justify-between px-4 py-3 border-b border-dark-700">
          <div className="flex items-center space-x-2">
            <Download className="w-4 h-4 text-accent-primary" />
            <h3 className="text-sm font-bold text-white">
              {status === 'completed' ? 'Download Complete' : 'Downloading...'}
            </h3>
          </div>
          <button 
            onClick={handleCancel}
            className="p-1 hover:bg-dark-700 rounded transition-colors"
          >
            <X className="w-4 h-4 text-gray-400" />
          </button>
        </div>

        {/* Content */}
        <div className="p-4 space-y-4">
          {/* Status */}
          <div className="flex items-center space-x-2">
            {status === 'downloading' && (
              <Loader2 className="w-4 h-4 text-accent-primary animate-spin" />
            )}
            <span className="text-sm text-gray-300">
              {status === 'preparing' && 'Preparing download...'}
              {status === 'downloading' && `Downloading ${filename || 'file'}...`}
              {status === 'completed' && 'Download completed successfully'}
              {status === 'error' && `Error: ${errorMessage}`}
            </span>
          </div>

          {/* Progress bar */}
          <div className="space-y-2">
            <div className="w-full h-2 bg-dark-950 rounded-full overflow-hidden">
              <div 
                className={cn(
                  "h-full transition-all duration-300",
                  status === 'error' ? "bg-red-500" : "bg-accent-primary"
                )}
                style={{ width: `${progress}%` }}
              />
            </div>
            <div className="flex justify-between text-xs text-gray-500">
              <span>{formatSize(received)} / {total > 0 ? formatSize(total) : '?'}</span>
              <span>{progress}%</span>
            </div>
          </div>

          {/* Speed */}
          {status === 'downloading' && (
            <div className="text-xs text-gray-500">
              Speed: {speed.toFixed(2)} KB/s
            </div>
          )}

          {/* File path */}
          {filename && (
            <div className="space-y-1">
              <label className="text-xs text-gray-500">File:</label>
              <div className="px-3 py-2 bg-dark-950 rounded text-sm text-gray-300 font-mono truncate">
                {filename}
              </div>
            </div>
          )}
        </div>

        {/* Buttons */}
        <div className="flex justify-end px-4 py-3 border-t border-dark-700">
          <button
            onClick={handleCancel}
            className={cn(
              "px-4 py-2 rounded text-sm transition-colors",
              status === 'completed' 
                ? "bg-accent-primary hover:bg-accent-primary/80 text-white"
                : "bg-dark-700 hover:bg-dark-600 text-gray-300"
            )}
          >
            {status === 'completed' ? 'Close' : 'Cancel'}
          </button>
        </div>
      </div>
    </div>
  );
};

export default DownloadProgressDialog;
