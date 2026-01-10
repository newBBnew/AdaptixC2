import React, { useState, useEffect } from 'react';
import Modal from '../../components/Modal';
import { cn } from '../../utils/cn';
import { 
  FileText, 
  X, 
  Loader2, 
  RefreshCw,
  Copy,
  Download,
  AlertCircle
} from 'lucide-react';
import { useAgents } from '../../context/AgentContext';

const FilePreviewDialog = ({ isOpen, onClose, agent, filePath, taskId }) => {
  const { tasks } = useAgents();
  const [content, setContent] = useState('');
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');

  useEffect(() => {
    if (!isOpen || !taskId) return;

    setLoading(true);
    setError('');
    setContent('');

    // Check if task is already completed
    const task = tasks[taskId];
    if (task && task.a_completed) {
      if (task.Status === 'Error') {
        setError(task.a_output || 'Failed to read file');
      } else {
        setContent(task.a_output || '');
      }
      setLoading(false);
    }
  }, [isOpen, taskId, tasks]);

  const handleCopy = () => {
    navigator.clipboard.writeText(content);
  };

  if (!isOpen) return null;

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title="Remote File Preview"
      width="max-w-4xl"
    >
      <div className="flex flex-col bg-theme-glass-panel h-[70vh]">
        {/* Header Info */}
        <div className="flex items-center justify-between px-6 py-4 border-b border-theme-glass-light bg-theme-glass shrink-0">
          <div className="flex items-center space-x-4">
            <div className="p-2 bg-theme-glass-panel border border-theme-glass-light rounded-lg">
              <FileText className="text-theme-accent" size={20} />
            </div>
            <div className="text-left">
              <p className="text-[10px] font-black uppercase tracking-widest text-theme-muted mb-0.5 tracking-tighter">
                {agent.a_computer} • TASK_{taskId?.substring(0,8)}
              </p>
              <p className="text-sm font-mono font-bold text-theme-primary truncate max-w-xl">{filePath}</p>
            </div>
          </div>
          
          <div className="flex items-center space-x-2">
            {!loading && !error && (
              <button 
                onClick={handleCopy}
                className="p-2 glass-btn text-theme-muted hover:text-theme-accent transition-all rounded-lg"
                title="Copy to clipboard"
              >
                <Copy size={16} />
              </button>
            )}
            <button 
              onClick={onClose} 
              className="p-2 text-theme-muted hover:text-theme-danger hover:bg-theme-danger/10 rounded-xl transition-all"
            >
              <X size={20} />
            </button>
          </div>
        </div>

        {/* Content Area */}
        <div className="flex-1 overflow-hidden relative flex flex-col">
          {loading ? (
            <div className="absolute inset-0 flex flex-col items-center justify-center space-y-4 bg-black/20 backdrop-blur-[1px]">
              <Loader2 className="w-10 h-10 text-theme-accent animate-spin" />
              <div className="text-center">
                <p className="text-[10px] font-black text-theme-muted uppercase tracking-[0.3em]">Retrieving_Block_Stream</p>
                <p className="text-[8px] text-theme-accent font-bold mt-1 uppercase tracking-widest animate-pulse">Waiting for agent callback...</p>
              </div>
            </div>
          ) : error ? (
            <div className="flex-1 flex flex-col items-center justify-center p-12 text-center space-y-4">
              <div className="p-4 bg-theme-danger/10 rounded-full border border-theme-danger/20">
                <AlertCircle className="w-12 h-12 text-theme-danger" />
              </div>
              <div>
                <h4 className="text-theme-primary font-bold">Access Denied or File Not Found</h4>
                <p className="text-sm text-theme-muted mt-2 font-mono max-w-md bg-black/20 p-3 rounded-lg border border-theme-glass-light">
                  {error}
                </p>
              </div>
              <button onClick={onClose} className="glass-btn px-8 py-2 text-xs font-black uppercase tracking-widest mt-4">
                Dismiss Portal
              </button>
            </div>
          ) : (
            <div className="flex-1 p-4 bg-black/40 overflow-auto custom-scrollbar">
              <pre className="font-mono text-[13px] text-theme-secondary leading-relaxed whitespace-pre-wrap break-all">
                {content || <span className="opacity-30 italic">Target file is empty</span>}
              </pre>
            </div>
          )}
        </div>

        {/* Footer */}
        <div className="px-4 py-2 border-t border-theme-glass-light bg-theme-glass shrink-0 flex items-center justify-between">
          <div className="flex items-center space-x-4">
            <div className="flex items-center space-x-2 text-[9px] font-black text-theme-muted uppercase tracking-widest">
              <span className="opacity-60">Status:</span>
              <span className={cn(loading ? "text-theme-accent animate-pulse" : error ? "text-theme-danger" : "text-theme-success")}>
                {loading ? "IN_FLIGHT" : error ? "FAILURE" : "SUCCESS_VERIFIED"}
              </span>
            </div>
          </div>
          <div className="text-[9px] font-mono text-theme-muted opacity-40">
            ENCRYPTED_IO_CHANNEL_ACTIVE
          </div>
        </div>
      </div>
    </Modal>
  );
};

export default FilePreviewDialog;
