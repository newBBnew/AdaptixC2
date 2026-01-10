import React, { useState, useEffect } from 'react';
import { 
  Shield, 
  Search, 
  Filter, 
  StopCircle, 
  Edit3, 
  RefreshCw,
  X,
  Globe,
  ArrowRight,
  Plus
} from 'lucide-react';
import { tunnelApi } from '../../api/control';
import { cn } from '../../utils/cn';

import CreateTunnelDialog from './CreateTunnelDialog';
import { useAgents } from '../../context/AgentContext';
import ContextMenu from '../../components/ContextMenu';

const TunnelsList = () => {
  const { tunnels, fetchAgents } = useAgents();
  const [loading, setLoading] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const [isCreateOpen, setIsCreateOpen] = useState(false);
  const [menu, setMenu] = useState(null);

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

  const handleSetInfo = async (id, currentInfo) => {
    const newInfo = window.prompt('Enter new info:', currentInfo || '');
    if (newInfo !== null) {
      try {
        await tunnelApi.setInfo(id, newInfo);
      } catch (err) {
        console.error('Failed to set info:', err);
      }
    }
  };

  const handleContextMenu = (e, tunnel) => {
    e.preventDefault();
    setMenu({
      x: e.clientX,
      y: e.clientY,
      options: [
        { label: 'Set Info...', icon: Edit3, onClick: () => handleSetInfo(tunnel.p_tunnel_id, tunnel.p_info) },
        { label: 'Stop Tunnel', icon: StopCircle, onClick: () => handleStopTunnel(tunnel.p_tunnel_id) },
      ]
    });
  };

  const filteredTunnels = tunnels.filter(t => 
    Object.values(t).some(val => 
      String(val).toLowerCase().includes(searchQuery.toLowerCase())
    )
  );

  const handleStopTunnel = async (tunnelId) => {
    if (!window.confirm('Are you sure you want to stop this tunnel?')) return;
    try {
      await tunnelApi.stop(tunnelId);
    } catch (err) {
      console.error('Failed to stop tunnel:', err);
    }
  };

  return (
    <div className="flex flex-col h-full w-full select-none overflow-hidden" onClick={() => setMenu(null)}>
      {/* 1. Header with Controls */}
      <div className="flex items-center justify-between px-3 py-2 glass-card-sm border-b border-theme-glass-light shrink-0">
        <div className="flex items-center space-x-3">
          <button 
            onClick={() => setIsSearchVisible(!isSearchVisible)}
            className={cn(
              "p-2 rounded-lg transition-colors",
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
            title="Refresh Tunnels"
          >
            <RefreshCw className={cn("w-4 h-4", loading && "animate-spin text-theme-accent")} />
          </button>
        </div>

        <div className="flex items-center">
          <button 
            onClick={() => setIsCreateOpen(true)}
            className="glass-btn-primary px-4 py-2 text-theme-primary flex items-center space-x-2 shadow-glow-sm"
          >
            <Plus className="w-4 h-4" />
            <span className="font-semibold text-sm">Create Tunnel</span>
          </button>
        </div>
      </div>

      {/* 2. Search Panel */}
      {isSearchVisible && (
        <div className="flex items-center px-4 py-2 glass-card-sm border-b border-theme-glass-light shrink-0">
          <div className="relative flex-1 max-w-md">
            <Filter className="w-4 h-4 absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted" />
            <input 
              type="text" 
              autoFocus
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search agent, port, info..." 
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
              <th className="w-32">Source Node</th>
              <th className="w-24">Type</th>
              <th className="w-40">Local Interface</th>
              <th className="w-24">Bind Port</th>
              <th className="w-40">Remote Target</th>
              <th>Operational Notes</th>
            </tr>
          </thead>
          <tbody className="text-[12px] font-medium">
            {filteredTunnels.length === 0 ? (
              <tr>
                <td colSpan="6" className="py-24 text-center border-none">
                  <div className="flex flex-col items-center space-y-4 opacity-40">
                    <Globe size={48} className="text-theme-muted" />
                    <p className="text-sm font-medium tracking-wider text-theme-muted">No active tunnels</p>
                  </div>
                </td>
              </tr>
            ) : (
              filteredTunnels.map((t) => (
                <tr 
                  key={t.p_tunnel_id} 
                  onContextMenu={(e) => handleContextMenu(e, t)}
                  className={cn(
                    "transition-colors group h-8 cursor-default",
                    t.p_agent_id === activeTabId ? "bg-theme-hover" : ""
                  )}
                >
                  <td className="text-theme-accent font-black font-mono uppercase tracking-tighter">{t.p_agent_id?.substring(0,8) || 'GLOBAL'}</td>
                  <td>
                    <span className="px-1.5 py-0.5 rounded-sm bg-theme-glass-panel text-[9px] font-black uppercase text-theme-muted border border-theme-glass-light">
                      {t.p_type || 'SOCKS5'}
                    </span>
                  </td>
                  <td className="text-theme-secondary font-mono">{t.p_interface || '0.0.0.0'}</td>
                  <td className="text-theme-accent-secondary font-black font-mono tracking-widest">{t.p_port}</td>
                  <td className="text-theme-primary font-mono">
                    {t.p_fhost ? (
                      <div className="flex items-center space-x-1.5">
                        <span>{t.p_fhost}</span>
                        <ChevronRight size={10} className="text-theme-muted" />
                        <span>{t.p_fport}</span>
                      </div>
                    ) : '---'}
                  </td>
                  <td className="text-theme-muted italic">{t.p_info || 'No operational data'}</td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>

      
      {/* 4. Footer Summary */}
      <div className="px-3 py-1.5 glass-card-sm border-t border-theme-glass-light flex items-center justify-between text-[9px] font-black text-theme-muted uppercase tracking-[0.1em] shrink-0">
        <div className="flex items-center space-x-6">
          <div className="flex items-center space-x-2 bg-theme-glass-panel px-3 py-1.5 rounded-lg border border-theme-glass-light shadow-sm">
            <span className="text-theme-muted opacity-60">ACTIVE_TUNNELS:</span>
            <span className="text-theme-accent font-mono font-bold">{tunnels.length}</span>
          </div>
        </div>
        <div className="flex items-center space-x-3 pr-1">
          <Shield size={12} className="text-theme-accent opacity-60" />
          <span className="text-theme-muted opacity-80">PIVOT_INFRASTRUCTURE_LINK</span>
          <div className="w-2 h-2 rounded-full bg-theme-success shadow-glow-sm animate-pulse" />
        </div>
      </div>

      <CreateTunnelDialog 
        isOpen={isCreateOpen}
        onClose={() => setIsCreateOpen(false)}
        onCreated={() => {}}
      />

      {/* Context Menu */}
      {menu && <ContextMenu {...menu} onClose={() => setMenu(null)} />}
    </div>
  );
};

export default TunnelsList;
