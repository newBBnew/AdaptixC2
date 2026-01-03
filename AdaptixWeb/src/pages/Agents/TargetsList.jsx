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
    <div className="flex flex-col h-full w-full select-none overflow-hidden" onClick={() => setMenu(null)}>
      {/* 1. Header with Controls */}
      <div className="flex items-center justify-between px-3 py-2 glass-card-sm border-b border-theme-glass-light shrink-0">
        <div className="flex items-center space-x-3">
          <button 
            onClick={() => setIsSearchVisible(!isSearchVisible)}
            className={cn(
              "p-2 rounded-xl transition-all",
              isSearchVisible ? "bg-theme-accent/20 text-theme-accent border border-theme-accent/30" : "text-theme-muted hover:text-theme-primary hover:bg-theme-hover"
            )}
            title="Toggle Search (Ctrl+F)"
          >
            <Search className="w-4 h-4 text-theme-accent" />
          </button>
          <div className="h-5 w-px bg-theme-glass-light mx-1" />
          <button 
            onClick={() => {
              setLoading(true);
              fetchAgents().finally(() => setLoading(false));
            }}
            className="p-2 glass-btn text-theme-muted hover:text-theme-accent transition-all"
            title="Refresh Targets"
          >
            <RefreshCw className={cn("w-4 h-4", loading && "animate-spin text-theme-accent")} />
          </button>
        </div>

        <div className="flex items-center">
          <button 
            onClick={() => {
              setEditData(null);
              setIsCreateOpen(true);
            }}
            className="glass-btn-primary px-4 py-2 text-white flex items-center space-x-2 shadow-glow-sm"
          >
            <Plus className="w-4 h-4" />
            <span className="font-semibold text-sm">Add Target</span>
          </button>
        </div>
      </div>

      {/* 2. Search Panel */}
      {isSearchVisible && (
        <div className="flex items-center px-4 py-2 glass-card-sm border-b border-theme-glass-light shrink-0 animate-in slide-in-from-top-1 duration-200">
          <div className="relative flex-1 max-w-md">
            <Filter className="w-4 h-4 absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted" />
            <input 
              type="text" 
              autoFocus
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search computer, domain, address..." 
              className="glass-input w-full pl-10 py-2 text-sm text-theme-primary placeholder:text-theme-muted"
            />
          </div>
        </div>
      )}

      {/* 3. Table Area */}
      <div className="flex-1 overflow-auto custom-scrollbar glass-panel">
        <table className="glass-table min-w-[900px]">
          <thead>
            <tr>
              <th className="w-48">Computer</th>
              <th className="w-40">Domain</th>
              <th className="w-40">Address</th>
              <th className="w-48">Operating System</th>
              <th className="w-32">Tag</th>
              <th>Operational Info</th>
              <th className="text-right">Actions</th>
            </tr>
          </thead>
          <tbody className="text-[11px] font-medium">
            {filteredTargets.length === 0 ? (
              <tr>
                <td colSpan="7" className="py-24 text-center border-none">
                  <div className="flex flex-col items-center space-y-4 opacity-20">
                    <Target size={48} className="text-theme-muted" />
                    <p className="text-[10px] font-black tracking-[0.2em] uppercase text-theme-muted">No telemetry targets found</p>
                  </div>
                </td>
              </tr>
            ) : (
              filteredTargets.map((t) => (
                <tr 
                  key={t.t_target_id} 
                  onContextMenu={(e) => handleContextMenu(e, t)}
                  onDoubleClick={() => handleEdit(t)}
                  className="transition-colors group h-10 cursor-default border-b border-theme-glass-light hover:bg-theme-glass"
                >
                  <td className="text-theme-accent font-black font-mono uppercase tracking-tight">{t.t_computer}</td>
                  <td className="text-theme-secondary font-mono italic">{t.t_domain || 'WORKGROUP'}</td>
                  <td className="text-theme-primary font-mono tracking-widest">{t.t_address}</td>
                  <td>
                    <div className="flex items-center space-x-2">
                      <Monitor className="w-3 h-3 text-theme-muted" />
                      <span className="text-theme-secondary text-[10px] uppercase truncate max-w-[150px]">{t.t_os_desk || 'UNKNOWN_ENV'}</span>
                    </div>
                  </td>
                  <td>
                    {t.t_tag ? (
                      <span className="px-1.5 py-0.5 rounded-sm bg-theme-accent/10 text-[9px] font-black uppercase text-theme-accent border border-theme-accent/20">
                        {t.t_tag}
                      </span>
                    ) : (
                      <span className="text-theme-muted text-[9px] font-black italic">UNTAGGED</span>
                    )}
                  </td>
                  <td className="text-theme-muted italic truncate max-w-xs group-hover:text-theme-primary transition-colors">{t.t_info || '---'}</td>
                  <td className="text-right">
                    <div className="flex items-center justify-end space-x-1 opacity-0 group-hover:opacity-100 transition-opacity">
                      <button className="p-1.5 rounded-lg hover:bg-theme-glass text-theme-muted hover:text-theme-primary transition-colors" title="Copy Identifier" onClick={() => navigator.clipboard.writeText(t.t_computer)}>
                        <Copy size={14} />
                      </button>
                      <button 
                        onClick={() => handleEdit(t)}
                        className="p-1.5 rounded-lg hover:bg-theme-glass text-theme-muted hover:text-theme-accent transition-colors" 
                        title="Edit Entry"
                      >
                        <Edit3 size={14} />
                      </button>
                      <button 
                        onClick={() => handleRemove(t.t_target_id)}
                        className="p-1.5 rounded-lg hover:bg-theme-glass text-theme-danger/70 hover:text-theme-danger transition-colors" 
                        title="Purge Entry"
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
      <div className="px-3 py-1.5 glass-card-sm border-t border-theme-glass-light flex items-center justify-between text-[9px] font-black text-theme-muted uppercase tracking-[0.15em] shrink-0">
        <div className="flex items-center space-x-6">
          <div className="flex items-center space-x-2 bg-theme-glass-panel px-3 py-1 rounded-lg border border-theme-glass-light shadow-glow-sm">
            <span className="opacity-60">OPERATIONAL_TARGETS:</span>
            <span className="text-theme-accent font-mono font-bold">{targets.length}</span>
          </div>
        </div>
        <div className="flex items-center space-x-3 pr-1">
          <Monitor size={10} className="text-theme-accent opacity-60" />
          <span className="text-theme-muted opacity-80 uppercase tracking-[0.2em]">Infrastructure_Nodes_Live</span>
          <div className="w-2 h-2 rounded-full bg-theme-success shadow-glow-sm animate-pulse" />
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
