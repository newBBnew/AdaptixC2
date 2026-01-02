import React, { useState, useEffect } from 'react';
import { 
  Target, 
  Search, 
  Filter, 
  Trash2, 
  Edit3, 
  Plus, 
  RefreshCw,
  X,
  Monitor,
  Copy,
  FileText,
  Tag
} from 'lucide-react';
import { dataApi } from '../../api/control';
import { cn } from '../../utils/cn';

import CreateTargetDialog from './CreateTargetDialog';
import { useAgents } from '../../context/AgentContext';
import ContextMenu from '../../components/ContextMenu';

const TargetsList = () => {
  const { targets, fetchAgents } = useAgents();
  const [loading, setLoading] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const [menu, setMenu] = useState(null);
  
  // Dialog states
  const [isCreateOpen, setIsCreateOpen] = useState(false);
  const [editData, setEditData] = useState(null);

  useEffect(() => {
    const handleKeyDown = (e) => {
      if ((e.ctrlKey || e.metaKey) && e.key === 'f') {
        e.preventDefault();
        setIsSearchVisible(prev => !prev);
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, []);

  const handleRemove = async (id) => {
    if (!window.confirm('Are you sure you want to remove this target?')) return;
    try {
      await dataApi.removeTarget([id]);
    } catch (err) {
      console.error('Failed to remove target:', err);
    }
  };

  const handleEdit = (target) => {
    setEditData(target);
    setIsCreateOpen(true);
  };

  const handleSetTag = async (target) => {
    const newTag = window.prompt('Enter new tag:', target.t_tag || '');
    if (newTag !== null) {
      try {
        await dataApi.editTarget({ ...target, t_tag: newTag });
        fetchAgents();
      } catch (err) {
        console.error('Failed to set tag:', err);
      }
    }
  };

  const handleContextMenu = (e, target) => {
    e.preventDefault();
    setMenu({
      x: e.clientX,
      y: e.clientY,
      options: [
        { label: 'Edit', icon: Edit3, onClick: () => handleEdit(target) },
        { label: 'Remove', icon: Trash2, onClick: () => handleRemove(target.t_target_id) },
        { divider: true },
        { label: 'Set Tag...', icon: Tag, onClick: () => handleSetTag(target) },
        { label: 'Export to file', icon: FileText, onClick: () => {
          const format = window.prompt('Format (use %computer%, %domain%, %address%, %os%):', '%computer%,%address%,%os%');
          if (!format) return;
          const text = format
            .replace(/%computer%/g, target.t_computer || '')
            .replace(/%domain%/g, target.t_domain || '')
            .replace(/%address%/g, target.t_address || '')
            .replace(/%os%/g, target.t_os_desk || '');
          const blob = new Blob([text], { type: 'text/plain' });
          const url = URL.createObjectURL(blob);
          const a = document.createElement('a');
          a.href = url;
          a.download = 'targets.txt';
          a.click();
          URL.revokeObjectURL(url);
        }},
        { label: 'Copy to clipboard', icon: Copy, onClick: () => {
          const text = `${target.t_computer} - ${target.t_address}`;
          navigator.clipboard.writeText(text);
        }},
      ]
    });
  };

  const filteredTargets = targets.filter(t => 
    Object.values(t).some(val => 
      String(val).toLowerCase().includes(searchQuery.toLowerCase())
    )
  );

  return (
    <div className="flex flex-col h-full w-full bg-dark-900 select-none overflow-hidden" onClick={() => setMenu(null)}>
      {/* 1. Header with Controls (Mimics TargetsWidget.cpp) */}
      <div className="flex items-center justify-between px-4 py-2 bg-dark-800 border-b border-dark-700 shrink-0">
        <div className="flex items-center space-x-3">
          <div className="flex items-center space-x-2 px-2 py-0.5 rounded bg-accent-primary/10 border border-accent-primary/20">
            <Target className="w-3.5 h-3.5 text-accent-primary" />
            <span className="text-[10px] font-black uppercase tracking-widest text-accent-primary">Target Data</span>
          </div>
          <div className="h-4 w-px bg-dark-600" />
          <button 
            onClick={() => setIsSearchVisible(!isSearchVisible)}
            className={cn(
              "p-1 rounded hover:bg-dark-700 transition-colors",
              isSearchVisible ? "bg-accent-primary/20 text-accent-primary" : "text-gray-500"
            )}
            title="Toggle Search (Ctrl+F)"
          >
            <Search className="w-3.5 h-3.5" />
          </button>
        </div>

        <div className="flex items-center space-x-1">
          <button 
            onClick={fetchAgents}
            className="p-1.5 rounded hover:bg-dark-700 text-gray-400 hover:text-white transition-all"
            title="Refresh"
          >
            <RefreshCw className={cn("w-3.5 h-3.5", false && "animate-spin text-accent-primary")} />
          </button>
          <div className="h-4 w-px bg-dark-600 mx-1" />
          <button 
            onClick={() => {
              setEditData(null);
              setIsCreateOpen(true);
            }}
            className="flex items-center space-x-1.5 px-3 py-1 rounded bg-accent-primary/10 border border-accent-primary/30 text-accent-primary hover:bg-accent-primary/20 transition-all group"
          >
            <Plus className="w-3.5 h-3.5 group-hover:scale-110 transition-transform" />
            <span className="text-[10px] font-bold uppercase">Add Target</span>
          </button>
        </div>
      </div>

      {/* 2. Search Panel */}
      {isSearchVisible && (
        <div className="flex items-center px-4 py-2 bg-dark-800/50 border-b border-dark-700 animate-in slide-in-from-top-2 duration-200 shrink-0">
          <div className="relative flex-1 max-w-md">
            <Filter className="w-3 h-3 absolute left-2.5 top-1/2 -translate-y-1/2 text-gray-600" />
            <input 
              type="text" 
              autoFocus
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="filter: (win | linux) & ^(test)" 
              className="w-full bg-dark-950/50 border border-dark-600 rounded px-8 py-1 text-[11px] text-gray-300 outline-none focus:border-accent-primary/50 placeholder:text-gray-700"
            />
          </div>
        </div>
      )}

      {/* 3. Table Area */}
      <div className="flex-1 overflow-auto scrollbar-thin">
        <table className="w-full text-left border-collapse table-auto min-w-[800px]">
          <thead className="sticky top-0 bg-dark-800 z-10 shadow-sm">
            <tr className="border-b border-dark-700 text-gray-500 text-[10px] font-bold uppercase tracking-tight">
              <th className="py-2 px-4 border-r border-dark-700/30">Computer</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Domain</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Address</th>
              <th className="py-2 px-4 border-r border-dark-700/30">OS</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Tag</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Info</th>
              <th className="py-2 px-4 text-right">Actions</th>
            </tr>
          </thead>
          <tbody className="text-[11px] font-medium divide-y divide-dark-800/30">
            {filteredTargets.length === 0 ? (
              <tr>
                <td colSpan="7" className="py-20 text-center text-gray-600 italic">
                  <div className="flex flex-col items-center space-y-3 opacity-20">
                    <Target size={40} />
                    <p className="text-xs font-medium tracking-widest uppercase">No targets identified</p>
                  </div>
                </td>
              </tr>
            ) : (
              filteredTargets.map((t) => (
                <tr 
                  key={t.t_target_id} 
                  className="hover:bg-accent-primary/5 transition-colors group h-8 cursor-default"
                  onContextMenu={(e) => handleContextMenu(e, t)}
                >
                  <td className="px-4 text-accent-primary font-bold truncate">{t.t_computer}</td>
                  <td className="px-4 text-gray-300 truncate">{t.t_domain || '---'}</td>
                  <td className="px-4 text-gray-300 font-mono truncate">{t.t_address}</td>
                  <td className="px-4 truncate">
                    <div className="flex items-center space-x-2">
                      <Monitor className="w-3 h-3 text-gray-500" />
                      <span className="text-gray-400 text-[10px]">{t.t_os_desk}</span>
                    </div>
                  </td>
                  <td className="px-4">
                    {t.t_tag && (
                      <span className="px-1.5 py-0.5 rounded bg-dark-700 text-[9px] font-black uppercase text-accent-secondary border border-accent-secondary/20">
                        {t.t_tag}
                      </span>
                    )}
                  </td>
                  <td className="px-4 text-gray-500 italic truncate max-w-xs">{t.t_info || '---'}</td>
                  <td className="px-4 text-right">
                    <div className="flex items-center justify-end space-x-1 opacity-0 group-hover:opacity-100 transition-opacity">
                      <button className="p-1 rounded hover:bg-dark-700 text-gray-400 hover:text-white transition-colors" title="Copy Info">
                        <Copy size={14} />
                      </button>
                      <button className="p-1 rounded hover:bg-dark-700 text-gray-400 hover:text-accent-secondary transition-colors" title="Set Tag">
                        <Tag size={14} />
                      </button>
                      <button 
                        onClick={() => handleEdit(t)}
                        className="p-1 rounded hover:bg-dark-700 text-gray-400 hover:text-white transition-colors" 
                        title="Edit"
                      >
                        <Edit3 size={14} />
                      </button>
                      <button 
                        onClick={() => handleRemove(t.t_target_id)}
                        className="p-1 rounded hover:bg-dark-700 text-accent-danger transition-colors" 
                        title="Remove"
                      >
                        <Trash2 size={14} />
                      </button>
                    </div>
                  </td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>
      
      {/* 4. Footer Summary */}
      <div className="px-4 py-1.5 bg-dark-800 border-t border-dark-700 flex items-center justify-between text-[10px] font-bold text-gray-500 uppercase tracking-tighter shrink-0">
        <div className="flex items-center space-x-4">
          <span>Total Targets: <span className="text-gray-300">{targets.length}</span></span>
        </div>
        <div className="flex items-center space-x-1">
          <button className="flex items-center space-x-1 hover:text-white transition-colors">
            <FileText size={10} />
            <span>Export to File</span>
          </button>
        </div>
      </div>

      <CreateTargetDialog 
        isOpen={isCreateOpen}
        onClose={() => setIsCreateOpen(false)}
        onSaved={() => {}}
        editMode={!!editData}
        initialData={editData}
      />

      {menu && (
        <ContextMenu
          x={menu.x}
          y={menu.y}
          options={menu.options}
          onClose={() => setMenu(null)}
        />
      )}
    </div>
  );
};

export default TargetsList;
