import React, { useState, useEffect, useMemo, useCallback } from 'react';
import { 
  Radio, 
  Search, 
  Filter, 
  Plus, 
  Edit3, 
  Trash2, 
  Cpu, 
  RefreshCw,
  X
} from 'lucide-react';
import { listenerApi } from '../../api/control';
import { cn } from '../../utils/cn';
import { useAgents } from '../../context/AgentContext';
import CreateListenerDialog from './CreateListenerDialog';
import GenerateAgentDialog from './GenerateAgentDialog';
import ContextMenu from '../../components/ContextMenu';

const ListenersList = () => {
  const { listeners, fetchAgents, globalSearchQuery } = useAgents();
  const [loading, setLoading] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const [menu, setMenu] = useState(null);
  
  const [isCreateOpen, setIsCreateOpen] = useState(false);
  const [isGenerateOpen, setIsGenerateOpen] = useState(false);
  const [selectedListenerId, setSelectedListenerId] = useState(null);
  const [selectedListener, setSelectedListener] = useState(null);
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

  const handleStopListener = useCallback(async (name, type) => {
    if (!window.confirm(`Are you sure you want to stop listener '${name}'?`)) return;
    try {
      await listenerApi.stop(name, type);
      await fetchAgents();
    } catch (err) {
      console.error('Failed to stop listener:', err);
    }
  }, [fetchAgents]);

  const handleEdit = useCallback((listener) => {
    setEditData(listener);
    setIsCreateOpen(true);
  }, []);

  const handleGenerate = useCallback((listener) => {
    setSelectedListener(listener);
    setIsGenerateOpen(true);
  }, []);

  const handleContextMenu = useCallback((e, listener) => {
    e.preventDefault();
    e.stopPropagation(); // Prevent bubbling
    setSelectedListenerId(listener.l_name); // Auto-select on right click
    setMenu({
      x: e.clientX,
      y: e.clientY,
      options: [
        { label: 'Create Listener', icon: Plus, onClick: () => { setEditData(null); setIsCreateOpen(true); } },
        { label: 'Edit Listener', icon: Edit3, onClick: () => handleEdit(listener) },
        { label: 'Stop Listener', icon: Trash2, color: 'text-theme-danger', onClick: () => handleStopListener(listener.l_name, listener.l_type) },
        { divider: true },
        { label: 'Generate Payload', icon: Cpu, onClick: () => handleGenerate(listener) },
      ]
    });
  }, [handleEdit, handleGenerate, handleStopListener]);

  const filteredListeners = useMemo(() => {
    const query = (searchQuery || globalSearchQuery).toLowerCase();
    return listeners.filter(l => 
      Object.values(l).some(val => 
        String(val).toLowerCase().includes(query)
      )
    );
  }, [listeners, searchQuery, globalSearchQuery]);

  const formatDate = (timestamp) => {
    if (!timestamp) return 'Just now';
    try {
      const d = new Date(timestamp * 1000);
      return isNaN(d.getTime()) ? 'Just now' : d.toLocaleString();
    } catch (e) {
      return 'Just now';
    }
  };

  return (
    <div className="flex flex-col h-full w-full overflow-hidden" onClick={() => setMenu(null)}>
      {/* 1. Header with Controls */}
      <div className="flex items-center justify-between px-3 py-2 glass-card-sm border-b border-theme-glass-light shrink-0 select-none">
        <div className="flex items-center space-x-3">
          <button 
            onClick={() => setIsSearchVisible(!isSearchVisible)}
            className={cn(
              "p-2 rounded-xl transition-all",
              isSearchVisible ? "bg-theme-accent/20 text-theme-accent border border-theme-accent/30" : "text-theme-muted hover:text-theme-primary hover:bg-theme-hover"
            )}
            title="Toggle Search (Ctrl+F)"
          >
            <Search className="w-4 h-4" />
          </button>
          <div className="h-5 w-px bg-theme-glass-light mx-1" />
          <button 
            onClick={() => {
              setLoading(true);
              fetchAgents().finally(() => setLoading(false));
            }}
            className="p-2 glass-btn text-theme-muted hover:text-theme-accent transition-all"
            title="Force Global Refresh"
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
            <span className="font-semibold text-sm">Create Listener</span>
          </button>
        </div>
      </div>

      {/* 2. Search Panel */}
      {isSearchVisible && (
        <div className="flex items-center px-4 py-2 glass-card-sm border-b border-theme-glass-light shrink-0 animate-in slide-in-from-top-1 duration-200 select-none">
          <div className="relative flex-1 max-w-md text-left">
            <Filter className="w-4 h-4 absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted" />
            <input 
              type="text" 
              autoFocus
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Filter by name, type, host..." 
              className="glass-input w-full pl-10 py-2 text-sm text-theme-primary placeholder:text-theme-muted"
            />
            {searchQuery && (
              <button 
                onClick={() => setSearchQuery('')}
                className="absolute right-3 top-1/2 -translate-y-1/2 text-theme-muted hover:text-theme-primary transition-colors"
              >
                <X className="w-4 h-4" />
              </button>
            )}
          </div>
        </div>
      )}

      {/* 3. Table Area */}
      <div className="flex-1 overflow-auto custom-scrollbar glass-panel">
        <table className="glass-table min-w-[900px]">
          <thead className="select-none">
            <tr>
              <th className="w-48">Name</th>
              <th className="w-24">Type</th>
              <th className="w-20">Proto</th>
              <th className="w-40">Bind Endpoint</th>
              <th className="w-48">Callback Address</th>
              <th className="w-40">Start Time</th>
              <th>Status</th>
            </tr>
          </thead>
          <tbody className="text-[12px] font-medium">
            {filteredListeners.length === 0 ? (
              <tr>
                <td colSpan="7" className="py-24 text-center border-none select-none">
                  <div className="flex flex-col items-center space-y-4 opacity-20">
                    <Radio size={64} className="text-theme-muted" />
                    <p className="text-[10px] font-black uppercase tracking-[0.3em] text-theme-muted">No active listeners identified</p>
                  </div>
                </td>
              </tr>
            ) : (
              filteredListeners.map((l) => (
                <tr 
                  key={l.l_name} 
                  onContextMenu={(e) => handleContextMenu(e, l)}
                  onClick={() => setSelectedListenerId(l.l_name)}
                  onDoubleClick={() => handleEdit(l)}
                  className={cn(
                    "transition-colors group h-10 cursor-pointer border-b border-theme-glass-light",
                    selectedListenerId === l.l_name 
                      ? "bg-theme-accent/10 hover:bg-theme-accent/20" 
                      : "hover:bg-theme-glass"
                  )}
                >
                  <td className="text-theme-accent font-black uppercase tracking-tight">{l.l_name}</td>
                  <td className="text-theme-primary font-mono text-[11px] font-bold">{l.l_reg_name || l.l_type}</td>
                  <td className="text-theme-secondary font-mono text-[11px] uppercase font-bold">{l.l_protocol || 'TCP'}</td>
                  <td className="text-theme-primary font-mono text-[11px] font-bold select-all">
                    {l.l_bind_host ? `${l.l_bind_host}:${l.l_bind_port}` : '0.0.0.0:0'}
                  </td>
                  <td className="text-theme-accent-secondary font-mono text-[11px] font-bold select-all truncate max-w-[200px]" title={l.l_agent_addr}>
                    {l.l_agent_addr || '---'}
                  </td>
                  <td className="text-theme-muted font-mono text-[11px]">{formatDate(l.l_create_time)}</td>
                  <td>
                    <div className="flex items-center space-x-2">
                      <div className={cn(
                        "w-2 h-2 rounded-full transition-all duration-300",
                         (l.l_status === 'active' || !l.l_status) ? "bg-theme-success shadow-glow-sm animate-pulse" : "bg-theme-muted opacity-40"
                      )} />
                      <span className={cn(
                        "text-[10px] font-black uppercase tracking-widest transition-colors",
                         (l.l_status === 'active' || !l.l_status) ? "text-theme-accent" : "text-theme-muted"
                      )}>
                        {(l.l_status || 'ACTIVE').toUpperCase()}
                      </span>
                    </div>
                  </td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>

      {/* 4. Footer Summary */}
      <div className="px-3 py-1.5 glass-card-sm border-t border-theme-glass-light flex items-center justify-between text-[9px] font-black text-theme-muted uppercase tracking-[0.15em] shrink-0 select-none">
        <div className="flex items-center space-x-6">
          <div className="flex items-center space-x-2 bg-theme-glass-panel px-3 py-1 rounded-lg border border-theme-glass-light shadow-glow-sm">
            <span className="opacity-60">OPERATIONAL_LISTENERS:</span>
            <span className="text-theme-accent font-mono">{listeners.length}</span>
          </div>
        </div>
        <div className="flex items-center space-x-3 pr-1">
          <span className="text-theme-muted opacity-80 uppercase tracking-[0.2em]">Edge_Comm_Ready</span>
          <div className="w-2 h-2 rounded-full bg-theme-success shadow-glow-sm animate-pulse" />
        </div>
      </div>

      {/* Dialogs */}
      <CreateListenerDialog 
        isOpen={isCreateOpen} 
        onClose={() => setIsCreateOpen(false)}
        onCreated={() => fetchAgents()}
        editMode={!!editData}
        initialData={editData}
      />

      <GenerateAgentDialog
        isOpen={isGenerateOpen}
        onClose={() => setIsGenerateOpen(false)}
        listenerName={selectedListener?.l_name}
        listenerType={selectedListener?.l_reg_name}
      />

      {/* Context Menu */}
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

export default ListenersList;
