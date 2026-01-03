import React, { useState, useEffect } from 'react';
import { X, Play, FileUp, Info, Settings, Code2 } from 'lucide-react';
import { cn } from '../../utils/cn';

const PluginDialog = ({ 
  isOpen, 
  onClose, 
  command,
  onExecute 
}) => {
  const [values, setValues] = useState({});

  useEffect(() => {
    if (command?.args) {
      const initial = {};
      command.args.forEach(arg => {
        if (arg.defaultValue !== undefined) {
          initial[arg.name] = arg.defaultValue;
        } else if (arg.type === 'bool') {
          initial[arg.name] = false;
        } else if (arg.type === 'int') {
          initial[arg.name] = 0;
        } else {
          initial[arg.name] = '';
        }
      });
      setValues(initial);
    }
  }, [command]);

  if (!isOpen || !command) return null;

  const handleChange = (name, value) => {
    setValues(prev => ({ ...prev, [name]: value }));
  };

  const buildCommandLine = () => {
    let cmdLine = command.name;
    
    command.args?.forEach(arg => {
      const value = values[arg.name];
      
      if (value === '' || value === undefined || value === null) {
        if (!arg.required) return;
      }
      
      if (arg.type === 'bool') {
        if (value && arg.mark) {
          cmdLine += ` ${arg.mark}`;
        }
        return;
      }
      
      const strValue = String(value);
      if (strValue === '' && !arg.required) return;
      
      if (arg.mark) {
        if (strValue.includes(' ')) {
          cmdLine += ` ${arg.mark} "${strValue}"`;
        } else {
          cmdLine += ` ${arg.mark} ${strValue}`;
        }
      } else {
        if (strValue.includes(' ')) {
          cmdLine += ` "${strValue}"`;
        } else {
          cmdLine += ` ${strValue}`;
        }
      }
    });
    
    return cmdLine;
  };

  const handleExecute = () => {
    const cmdLine = buildCommandLine();
    onExecute?.(cmdLine, values);
    onClose();
  };

  const renderInput = (arg) => {
    const value = values[arg.name];

    if (arg.type === 'file') {
      return (
        <div className="flex items-center space-x-2">
          <input
            type="text"
            value={value || ''}
            onChange={(e) => handleChange(arg.name, e.target.value)}
            placeholder={arg.mark || 'Select file...'}
            className="flex-1 glass-input font-mono py-2 px-4 text-theme-primary"
          />
          <label className="p-2.5 bg-theme-glass border border-theme-glass-light hover:border-theme-accent rounded-xl cursor-pointer text-theme-muted hover:text-theme-accent transition-all shadow-sm">
            <FileUp size={16} />
            <input
              type="file"
              className="hidden"
              onChange={(e) => {
                const file = e.target.files?.[0];
                if (file) handleChange(arg.name, file.name);
              }}
            />
          </label>
        </div>
      );
    }

    if (arg.type === 'int') {
      return (
        <input
          type="number"
          value={value || 0}
          onChange={(e) => handleChange(arg.name, parseInt(e.target.value) || 0)}
          className="glass-input w-full font-mono text-center py-2 px-4 text-theme-primary"
        />
      );
    }

    if (arg.type === 'bool') {
      return (
        <label className="flex items-center space-x-3 cursor-pointer group">
          <input
            type="checkbox"
            checked={!!value}
            onChange={(e) => handleChange(arg.name, e.target.checked)}
            className="sr-only"
          />
          <div className={cn(
            "w-5 h-5 border border-theme-glass-light rounded-lg flex items-center justify-center transition-all",
            value ? "bg-theme-accent border-theme-accent shadow-glow-sm" : "bg-theme-glass group-hover:border-theme-accent/50"
          )}>
            {value && <div className="w-2 h-2 bg-theme-primary rounded-full shadow-sm" />}
          </div>
          <span className="text-[11px] font-bold text-theme-secondary uppercase tracking-tight">Enable Parameter</span>
        </label>
      );
    }

    // Default: string
    return (
      <input
        type="text"
        value={value || ''}
        onChange={(e) => handleChange(arg.name, e.target.value)}
        placeholder={arg.mark || ''}
        className="glass-input w-full font-mono py-2 px-4 text-theme-primary"
      />
    );
  };

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title={`Execute Plugin: ${command.name}`}
      width="max-w-2xl"
    >
      <div className="flex flex-col bg-theme-glass-panel">
        <div className="max-h-[70vh] overflow-y-auto custom-scrollbar bg-theme-glass-panel">
          {/* Parameters */}
          {command.args?.length > 0 && (
            <div className="p-6 space-y-6">
              <div className="flex items-center space-x-2 text-[10px] font-black text-theme-muted uppercase tracking-[0.2em] mb-2 border-b border-theme-glass-light pb-2">
                <Settings size={14} className="text-theme-accent" />
                <span>Command Parameters</span>
              </div>
              <div className="grid grid-cols-1 gap-6">
                {command.args.map((arg) => (
                  <div key={arg.name} className="space-y-2">
                    <label className="block text-[10px] font-black text-theme-muted uppercase tracking-widest ml-1">
                      {arg.name}
                      {arg.required && <span className="text-theme-danger ml-1">*</span>}
                      {arg.description && (
                        <span className="text-theme-muted text-[9px] ml-2 normal-case font-bold italic opacity-60">({arg.description})</span>
                      )}
                    </label>
                    <div className="w-full">
                      {renderInput(arg)}
                    </div>
                  </div>
                ))}
              </div>
            </div>
          )}

          {/* Metadata Section (Description & Example) */}
          <div className="p-6 pt-0 space-y-6">
            {command.description && (
              <div className="space-y-2">
                <div className="text-[10px] font-black text-theme-muted uppercase tracking-widest ml-1">Description</div>
                <div className="p-4 bg-theme-glass border border-theme-glass-light rounded-2xl text-[11px] text-theme-secondary leading-relaxed shadow-glow-sm">
                  {command.description}
                </div>
              </div>
            )}

            {command.example && (
              <div className="space-y-2">
                <div className="text-[10px] font-black text-theme-muted uppercase tracking-widest ml-1">Usage Example</div>
                <div className="p-4 bg-theme-glass border border-theme-glass-light rounded-2xl font-mono text-[10px] text-theme-accent shadow-glow-sm">
                  {command.example}
                </div>
              </div>
            )}

            {/* Live Preview */}
            <div className="space-y-2">
              <div className="text-[10px] font-black text-theme-muted uppercase tracking-widest flex items-center justify-between ml-1 pr-1">
                <span>Final Command Preview</span>
                <span className="text-theme-accent-secondary opacity-50 font-mono text-[9px]">READY</span>
              </div>
              <div className="p-4 bg-theme-glass-panel/40 border border-theme-glass-light rounded-2xl font-mono text-[10px] text-theme-success break-all shadow-glow-sm">
                <span className="text-theme-muted mr-2">$</span>{buildCommandLine()}
              </div>
            </div>
          </div>
        </div>

        {/* Buttons */}
        <div className="flex justify-end space-x-3 p-6 bg-theme-glass-panel border-t border-theme-glass-light">
          <button
            onClick={onClose}
            className="glass-btn px-6 py-2 text-xs font-bold text-theme-muted hover:text-theme-primary transition-all uppercase tracking-widest"
          >
            Cancel
          </button>
          <button
            onClick={handleExecute}
            className="glass-btn-primary px-8 py-2 text-xs font-black uppercase tracking-widest shadow-glow-sm hover:shadow-glow flex items-center space-x-2 text-white"
          >
            <Play className="w-3 h-3 text-white" />
            <span>Execute</span>
          </button>
        </div>
      </div>
    </Modal>
  );
};

export default PluginDialog;
