import React, { useState, useMemo } from 'react';
import { 
  Wind, 
  Search, 
  RefreshCw, 
  Trash2, 
  ChevronRight, 
  Terminal, 
  Monitor,
  Network,
  X,
  Plus,
  ArrowRight,
  Shield
} from 'lucide-react';
import { cn } from '../../utils/cn';
import { useAgents } from '../../context/AgentContext';
import { pivotApi } from '../../api/control';
import ContextMenu from '../../components/ContextMenu';

const PivotsList = () => {
  const { pivots, agents, openAgentTab, globalSearchQuery } = useAgents();
  const [searchQuery, setSearchQuery] = useState('');
  const [menu, setMenu] = useState(null);

  const pivotsArray = useMemo(() => Object.values(pivots), [pivots]);

  const filteredPivots = useMemo(() => {
    const query = (searchQuery || globalSearchQuery).toLowerCase();
    return pivotsArray.filter(p => 
      p.p_pivot_name?.toLowerCase().includes(query) ||
      p.p_parent_agent_id?.toLowerCase().includes(query) ||
      p.p_child_agent_id?.toLowerCase().includes(query)
    );
  }, [pivotsArray, searchQuery, globalSearchQuery]);

  const getAgentName = (id) => {
    const agent = agents.find(a => a.a_id === id);
    return agent ? `${agent.a_computer} (${agent.a_username})` : id.substring(0, 8);
  };

  const handleRemovePivot = async (pivotId) => {
    if (window.confirm('Are you sure you want to terminate this cascade relay?')) {
      try {
        await pivotApi.remove(pivotId);
      } catch (err) {
        console.error('Failed to remove pivot:', err);
      }
    }
  };

  const handleContextMenu = (e, pivot) => {
    e.preventDefault();
    setMenu({
      x: e.clientX,
      y: e.clientY,
      options: [
        { label: 'Interact Parent', icon: Terminal, onClick: () => {
          const agent = agents.find(a => a.a_id === pivot.p_parent_agent_id);
          if (agent) openAgentTab(agent, 'console');
        }},
        { label: 'Interact Child', icon: Terminal, onClick: () => {
          const agent = agents.find(a => a.a_id === pivot.p_child_agent_id);
          if (agent) openAgentTab(agent, 'console');
        }},
        { divider: true },
        { label: 'Remove Pivot', icon: Trash2, color: 'text-theme-danger', onClick: () => handleRemovePivot(pivot.p_pivot_id) },
      ]
    });
  };

  return (
    <div className="flex flex-col h-full w-full select-none overflow-hidden" onClick={() => setMenu(null)}>
      {/* 1. Header */}
      <div className="flex items-center justify-between px-3 py-2 glass-card-sm border-b border-theme-glass-light shrink-0">
        <div className="flex items-center space-x-3">
          <div className="flex items-center space-x-2 bg-theme-glass-panel px-3 py-1.5 rounded-lg border border-theme-glass-light shadow-glow-sm">
            <Network size={14} className="text-theme-accent" />
            <span className="text-[10px] font-black uppercase tracking-widest text-theme-muted">Cascade Pivots</span>
          </div>
        </div>
      </div>

      {/* 2. Search Panel */}
      <div className="px-4 py-2 glass-card-sm border-b border-theme-glass-light flex items-center space-x-4 shrink-0">
        <div className="relative flex-1 max-w-md">
          <Search className="w-4 h-4 absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted" />
          <input 
            type="text" 
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="Search pivots, node IDs..." 
            className="glass-input w-full pl-10 py-2 text-sm text-theme-primary placeholder:text-theme-muted"
          />
        </div>
      </div>

      {/* 3. Table Area */}
      <div className="flex-1 overflow-auto custom-scrollbar glass-panel">
        <table className="glass-table min-w-full">
          <thead>
            <tr>
              <th className="w-48">Pivot Alias</th>
              <th>Infrastructure Link</th>
              <th className="text-right">Actions</th>
            </tr>
          </thead>
          <tbody className="text-[12px] font-medium">
            {filteredPivots.length === 0 ? (
              <tr>
                <td colSpan="3" className="py-24 text-center border-none">
                  <div className="flex flex-col items-center space-y-4 opacity-20">
                    <Network size={48} className="text-theme-muted" />
                    <p className="text-[10px] font-black tracking-[0.2em] uppercase text-theme-muted">No cascade links established</p>
                  </div>
                </td>
              </tr>
            ) : (
              filteredPivots.map((p) => (
                <tr 
                  key={p.p_pivot_id} 
                  onContextMenu={(e) => handleContextMenu(e, p)}
                  className="transition-colors group h-12 cursor-default border-b border-theme-glass-light hover:bg-theme-glass"
                >
                  <td className="text-theme-accent font-black font-mono uppercase tracking-tight pl-4">
                    {p.p_pivot_name}
                  </td>
                  <td>
                    <div className="flex items-center space-x-4">
                      <div className="flex flex-col items-end min-w-[120px]">
                        <span className="text-theme-primary text-[11px] font-bold truncate max-w-[150px]">{getAgentName(p.p_parent_agent_id)}</span>
                        <span className="text-theme-muted text-[9px] font-mono">{p.p_parent_agent_id.substring(0,8)}</span>
                      </div>
                      
                      <div className="flex flex-col items-center px-2">
                        <div className="flex items-center space-x-1">
                          <div className="w-8 h-px bg-gradient-to-r from-theme-accent to-theme-accent-secondary" />
                          <ArrowRight size={12} className="text-theme-accent-secondary animate-pulse" />
                          <div className="w-8 h-px bg-gradient-to-r from-theme-accent-secondary to-theme-accent" />
                        </div>
                        <span className="text-[8px] font-black text-theme-accent mt-1 tracking-tighter">RELAY</span>
                      </div>

                      <div className="flex flex-col items-start min-w-[120px]">
                        <span className="text-theme-primary text-[11px] font-bold truncate max-w-[150px]">{getAgentName(p.p_child_agent_id)}</span>
                        <span className="text-theme-muted text-[9px] font-mono">{p.p_child_agent_id.substring(0,8)}</span>
                      </div>
                    </div>
                  </td>
                      <td className="text-right pr-4">
                        <div className="flex items-center justify-end space-x-1 opacity-0 group-hover:opacity-100 transition-opacity">
                          <button 
                            onClick={() => {
                              const agent = agents.find(a => a.a_id === p.p_child_agent_id);
                              if (agent) openAgentTab(agent, 'console');
                            }}
                            className="p-1.5 rounded-lg hover:bg-theme-glass text-theme-muted hover:text-theme-accent transition-colors"
                            title="Interact with Child"
                          >
                            <Terminal size={14} />
                          </button>
                          <button 
                            onClick={() => handleRemovePivot(p.p_pivot_id)}
                            className="p-1.5 rounded-lg hover:bg-theme-glass text-theme-danger/70 hover:text-theme-danger transition-colors"
                            title="Remove Pivot"
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
          <div className="flex items-center space-x-2 bg-theme-glass-panel px-3 py-1 rounded-lg border border-theme-glass-light">
            <span className="opacity-60">ACTIVE_RELAYS:</span>
            <span className="text-theme-accent font-mono">{pivotsArray.length}</span>
          </div>
        </div>
        <div className="flex items-center space-x-3 pr-1">
          <Shield size={10} className="text-theme-accent opacity-60" />
          <span className="text-theme-muted opacity-80">Encrypted_Multi_Hop_Link</span>
          <div className="w-2 h-2 rounded-full bg-theme-success shadow-glow-sm animate-pulse" />
        </div>
      </div>

      {/* Context Menu */}
      {menu && <ContextMenu {...menu} onClose={() => setMenu(null)} />}
    </div>
  );
};

export default PivotsList;
