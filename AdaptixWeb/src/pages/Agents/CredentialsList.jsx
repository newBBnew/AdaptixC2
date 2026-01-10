import React, { useState, useEffect } from 'react';
import { 
  Key, 
  Search, 
  Filter, 
  Trash2, 
  Edit3, 
  Plus, 
  RefreshCw,
  X,
  Copy,
  Tag,
  Shield,
  User,
  Database,
  FileDown
} from 'lucide-react';
import { dataApi } from '../../api/control';
import { cn } from '../../utils/cn';

import CreateCredentialDialog from './CreateCredentialDialog';
import { useAgents } from '../../context/AgentContext';
import ContextMenu from '../../components/ContextMenu';

const CredentialsList = () => {
  const { credentials, fetchAgents, globalSearchQuery } = useAgents();
  const [loading, setLoading] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const [filterType, setFilterType] = useState('All Types');
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
    if (!window.confirm('Are you sure you want to remove this credential?')) return;
    try {
      await dataApi.removeCred([id]);
      fetchAgents();
    } catch (err) {
      console.error('Failed to remove credential:', err);
    }
  };

  const handleEdit = (cred) => {
    setEditData(cred);
    setIsCreateOpen(true);
  };

  const credTypes = ['All Types', ...new Set(credentials.map(c => c.type).filter(Boolean))].sort();

  const filteredCreds = credentials.filter(c => {
    const query = (searchQuery || globalSearchQuery).toLowerCase();
    const matchesSearch = Object.values(c).some(val => 
      String(val).toLowerCase().includes(query)
    );
    const matchesType = filterType === 'All Types' || c.c_type === filterType;
    return matchesSearch && matchesType;
  });

  const handleCopyPassword = (pass) => {
    navigator.clipboard.writeText(pass);
  };

  const handleCopyCommand = (cred, format) => {
    let text = '';
    const { c_username: username, c_password: password, c_realm: realm } = cred;
    switch (format) {
      case 'realm_user_pass': text = `${realm}\\${username}:${password}`; break;
      case 'user': text = username; break;
      case 'pass': text = password; break;
      case 'impacket_1': text = `'${realm}/${username}:${password}'`; break;
      case 'impacket_2': text = `-hashes :${password} '${realm}/${username}'`; break;
      case 'netexec_1': text = `-u '${username}' -p '${password}'`; break;
      case 'netexec_2': text = `-u '${username}' -H '${password}'`; break;
      case 'certipy': text = `-u '${username}@${realm}' -p '${password}'`; break;
      default: text = password;
    }
    navigator.clipboard.writeText(text);
  };

  const handleSetTag = async (id, currentTag) => {
    const newTag = window.prompt('Enter new tag:', currentTag || '');
    if (newTag !== null) {
      try {
        await dataApi.setCredTag([id], newTag);
        fetchAgents();
      } catch (err) {
        console.error('Failed to set tag:', err);
      }
    }
  };

  const handleContextMenu = (e, cred) => {
    e.preventDefault();
    setMenu({
      x: e.clientX,
      y: e.clientY,
      options: [
        { label: 'Edit Credential', icon: Edit3, onClick: () => handleEdit(cred) },
        { label: 'Remove Credential', icon: Trash2, onClick: () => handleRemove(cred.c_creds_id) },
        { divider: true },
        { label: 'Set Tag...', icon: Tag, onClick: () => handleSetTag(cred.c_creds_id, cred.c_tag) },
        { label: 'Export to file', icon: FileDown, onClick: () => {
          const format = window.prompt('Format (use %realm%, %username%, %password%):', '%realm%\\%username%:%password%');
          if (!format) return;
          const text = format
            .replace(/%realm%/g, cred.c_realm || '')
            .replace(/%username%/g, cred.c_username || '')
            .replace(/%password%/g, cred.c_password || '');
          const blob = new Blob([text], { type: 'text/plain' });
          const url = URL.createObjectURL(blob);
          const a = document.createElement('a');
          a.href = url;
          a.download = 'creds.txt';
          a.click();
          URL.revokeObjectURL(url);
        }},
        { 
          label: 'Copy to Clipboard', 
          icon: Copy,
          children: [
            { label: 'Realm\\User:Pass', onClick: () => handleCopyCommand(cred, 'realm_user_pass') },
            { label: 'User only', onClick: () => handleCopyCommand(cred, 'user') },
            { label: 'Password/Hash only', onClick: () => handleCopyCommand(cred, 'pass') },
            { divider: true },
            { label: 'Impacket Format', onClick: () => handleCopyCommand(cred, 'impacket_1') },
            { label: 'NetExec (-p)', onClick: () => handleCopyCommand(cred, 'netexec_1') },
            { label: 'NetExec (-H)', onClick: () => handleCopyCommand(cred, 'netexec_2') },
            { label: 'Certipy', onClick: () => handleCopyCommand(cred, 'certipy') },
          ]
        },
      ]
    });
  };

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
            <Search className="w-4 h-4" />
          </button>
          <div className="h-5 w-px bg-theme-glass-light mx-1" />
          <button 
            onClick={() => {
              setLoading(true);
              fetchAgents().finally(() => setLoading(false));
            }}
            className="p-2 glass-btn text-theme-muted hover:text-theme-accent transition-all"
            title="Refresh Credentials"
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
            className="glass-btn-primary px-4 py-2 text-theme-primary flex items-center space-x-2 shadow-glow-sm"
          >
            <Plus className="w-4 h-4" />
            <span className="font-semibold text-sm">Import Credential</span>
          </button>
        </div>
      </div>

      {/* 2. Search Panel */}
      {isSearchVisible && (
        <div className="flex items-center px-4 py-2 glass-card-sm border-b border-theme-glass-light shrink-0 space-x-3">
          <div className="relative flex-1 max-w-md">
            <Filter className="w-4 h-4 absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted" />
            <input 
              type="text" 
              autoFocus
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search user, realm, password..." 
              className="glass-input w-full pl-10 py-2 text-sm text-theme-primary placeholder:text-theme-muted"
            />
          </div>
          <div className="flex items-center glass-input rounded-lg px-3 py-1.5 shrink-0">
            <span className="text-[10px] font-black text-theme-muted uppercase mr-2">Category:</span>
            <select 
              value={filterType}
              onChange={(e) => setFilterType(e.target.value)}
              className="bg-transparent text-[10px] font-bold text-theme-primary outline-none cursor-pointer"
            >
              {credTypes.map(t => <option key={t} value={t} className="bg-theme-glass-panel text-theme-primary">{t}</option>)}
            </select>
          </div>
        </div>
      )}

      {/* 3. Table Area */}
      <div className="flex-1 overflow-auto custom-scrollbar glass-panel">
        <table className="glass-table min-w-[1000px]">
          <thead>
            <tr>
              <th className="w-48">Operator / User</th>
              <th className="w-64">Credential Material</th>
              <th className="w-48">Domain / Realm</th>
              <th className="w-32">Type</th>
              <th className="w-32">Source Node</th>
              <th>Artifact Tag</th>
              <th className="text-right">Actions</th>
            </tr>
          </thead>
          <tbody className="text-[11px] font-medium text-theme-primary">
            {filteredCreds.length === 0 ? (
              <tr>
                <td colSpan="7" className="py-24 text-center border-none">
                  <div className="flex flex-col items-center space-y-4 opacity-40">
                    <Key size={48} className="text-theme-muted" />
                    <p className="text-[10px] font-black tracking-[0.2em] uppercase text-theme-muted">No telemetry loot recovered</p>
                  </div>
                </td>
              </tr>
            ) : (
              filteredCreds.map((c) => (
                <tr 
                  key={c.c_creds_id} 
                  onContextMenu={(e) => handleContextMenu(e, c)}
                  className="transition-colors group h-8 cursor-default hover:bg-theme-hover"
                >
                  <td className="text-theme-accent font-black font-mono">
                    <div className="flex items-center space-x-2">
                      <User size={12} className="text-theme-muted" />
                      <span>{c.c_username}</span>
                    </div>
                  </td>
                  <td className="text-theme-primary font-mono select-text cursor-pointer hover:text-theme-accent transition-colors" onClick={() => handleCopyPassword(c.c_password)}>
                    <div className="flex items-center justify-between">
                      <span className="truncate max-w-[200px]">{c.c_password}</span>
                      <Copy size={10} className="text-theme-muted opacity-0 group-hover:opacity-100" />
                    </div>
                  </td>
                  <td className="text-theme-secondary font-mono">{c.c_realm || 'LOCAL_DB'}</td>
                  <td>
                    <span className="px-1.5 py-0.5 rounded-sm bg-theme-glass-panel text-[9px] font-black uppercase text-theme-muted border border-theme-glass-light">
                      {c.c_type}
                    </span>
                  </td>
                  <td className="text-theme-secondary text-[10px] font-mono">
                    <div className="flex items-center space-x-1.5">
                      <Database size={10} className="text-theme-muted" />
                      <span>{c.c_host || (c.c_agent_id ? c.c_agent_id.substring(0,8) : 'MANUAL')}</span>
                    </div>
                  </td>
                  <td>
                    {c.c_tag ? (
                      <span className="px-1.5 py-0.5 rounded-sm bg-theme-glass-panel text-[9px] font-black uppercase text-theme-accent-secondary border border-theme-glass-light">
                        {c.c_tag}
                      </span>
                    ) : (
                      <span className="text-theme-muted text-[9px] font-black italic opacity-30">UNTAGGED</span>
                    )}
                  </td>
                  <td className="text-right">
                    <div className="flex items-center justify-end space-x-1 opacity-0 group-hover:opacity-100 transition-opacity pr-2">
                      <button onClick={() => handleEdit(c)} className="p-1 rounded hover:bg-theme-hover text-theme-muted hover:text-theme-accent transition-colors"><Edit3 size={14} /></button>
                      <button onClick={() => handleRemove(c.c_creds_id)} className="p-1 rounded hover:bg-theme-hover text-theme-muted hover:text-theme-danger transition-colors"><Trash2 size={14} /></button>
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
            <span className="opacity-60">LOOT_RECOVERED:</span>
            <span className="text-theme-accent font-mono font-bold">{credentials.length}</span>
          </div>
        </div>
        <div className="flex items-center space-x-3 pr-1">
          <span className="text-theme-muted opacity-80 uppercase tracking-[0.2em]">Encrypted_Database_Sync</span>
          <div className="w-2 h-2 rounded-full bg-theme-accent shadow-glow-sm animate-pulse" />
        </div>
      </div>

      <CreateCredentialDialog 
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

export default CredentialsList;
