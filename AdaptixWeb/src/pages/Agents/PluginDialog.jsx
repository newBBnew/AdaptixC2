import React, { useState, useEffect } from 'react';
import { X, Play, FileUp, Info } from 'lucide-react';
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
            className="flex-1 px-3 py-2 bg-dark-950 border border-dark-600 rounded text-sm text-white outline-none focus:border-accent-primary"
          />
          <label className="px-3 py-2 bg-dark-700 hover:bg-dark-600 rounded cursor-pointer text-sm text-gray-300 transition-colors">
            <FileUp className="w-4 h-4" />
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
          className="w-full px-3 py-2 bg-dark-950 border border-dark-600 rounded text-sm text-white outline-none focus:border-accent-primary"
        />
      );
    }

    if (arg.type === 'bool') {
      return (
        <label className="flex items-center space-x-2 cursor-pointer">
          <input
            type="checkbox"
            checked={!!value}
            onChange={(e) => handleChange(arg.name, e.target.checked)}
            className="w-4 h-4 rounded border-dark-600 bg-dark-950 text-accent-primary focus:ring-accent-primary"
          />
          <span className="text-sm text-gray-400">Enable</span>
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
        className="w-full px-3 py-2 bg-dark-950 border border-dark-600 rounded text-sm text-white outline-none focus:border-accent-primary"
      />
    );
  };

  return (
    <div className="fixed inset-0 bg-black/60 flex items-center justify-center z-50">
      <div className="bg-dark-800 border border-dark-600 rounded-lg shadow-xl w-full max-w-md">
        {/* Header */}
        <div className="flex items-center justify-between px-4 py-3 border-b border-dark-700">
          <h3 className="text-sm font-bold text-white">{command.name}</h3>
          <button 
            onClick={onClose}
            className="p-1 hover:bg-dark-700 rounded transition-colors"
          >
            <X className="w-4 h-4 text-gray-400" />
          </button>
        </div>

        {/* Parameters */}
        {command.args?.length > 0 && (
          <div className="p-4 space-y-4">
            <div className="text-[10px] font-bold text-gray-500 uppercase tracking-widest mb-3">
              Parameters
            </div>
            {command.args.map((arg) => (
              <div key={arg.name} className="space-y-1">
                <label className="block text-sm text-gray-300">
                  {arg.name}
                  {arg.required && <span className="text-red-400 ml-1">*</span>}
                  {arg.description && (
                    <span className="text-gray-500 text-xs ml-2">({arg.description})</span>
                  )}
                </label>
                {renderInput(arg)}
              </div>
            ))}
          </div>
        )}

        {/* Description */}
        {command.description && (
          <div className="px-4 pb-4">
            <div className="text-[10px] font-bold text-gray-500 uppercase tracking-widest mb-2">
              Description
            </div>
            <div className="p-3 bg-dark-900 rounded text-sm text-gray-400 border border-dark-700">
              {command.description}
            </div>
          </div>
        )}

        {/* Example */}
        {command.example && (
          <div className="px-4 pb-4">
            <div className="text-[10px] font-bold text-gray-500 uppercase tracking-widest mb-2">
              Example
            </div>
            <div className="p-2 bg-dark-950 rounded font-mono text-xs text-accent-primary border border-dark-700">
              {command.example}
            </div>
          </div>
        )}

        {/* Preview */}
        <div className="px-4 pb-4">
          <div className="text-[10px] font-bold text-gray-500 uppercase tracking-widest mb-2">
            Command Preview
          </div>
          <div className="p-2 bg-dark-950 rounded font-mono text-xs text-green-400 border border-dark-700 break-all">
            {buildCommandLine()}
          </div>
        </div>

        {/* Buttons */}
        <div className="flex justify-end space-x-2 px-4 py-3 border-t border-dark-700">
          <button
            onClick={onClose}
            className="px-4 py-2 text-sm text-gray-400 hover:text-white transition-colors"
          >
            Cancel
          </button>
          <button
            onClick={handleExecute}
            className="flex items-center space-x-2 px-4 py-2 bg-accent-primary hover:bg-accent-primary/80 text-white rounded text-sm transition-colors"
          >
            <Play className="w-4 h-4" />
            <span>Execute</span>
          </button>
        </div>
      </div>
    </div>
  );
};

export default PluginDialog;
