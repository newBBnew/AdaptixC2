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
  const { credentials, fetchAgents } = useAgents();
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
    const matchesSearch = Object.values(c).some(val => 
      String(val).toLowerCase().includes(searchQuery.toLowerCase())
    );
    const matchesType = filterType === 'All Types' || c.type === filterType;
    return matchesSearch && matchesType;
  });

  const handleCopyPassword = (pass) => {
    navigator.clipboard.writeText(pass);
  };

  const handleCopyCommand = (cred, format) => {
    let text = '';
    const { username, password, realm } = cred;
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
    // Optional: add a toast notification here
  };

  const handleSetTag = async (id, currentTag) => {
    const newTag = window.prompt('Enter new tag:', currentTag || '');
    if (newTag !== null) {
      try {
        await dataApi.editCred({ ...credentials.find(c => c.cred_id === id), tag: newTag });
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
        { label: 'Remove Credential', icon: Trash2, onClick: () => handleRemove(cred.cred_id) },
        { divider: true },
        { label: 'Set Tag...', icon: Tag, onClick: () => handleSetTag(cred.cred_id, cred.tag) },
        { label: 'Export to file', icon: FileDown, onClick: () => {
          const format = window.prompt('Format (use %realm%, %username%, %password%):', '%realm%\\%username%:%password%');
          if (!format) return;
          const text = format
            .replace(/%realm%/g, cred.realm || '')
            .replace(/%username%/g, cred.username || '')
            .replace(/%password%/g, cred.password || '');
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
    <div className="flex flex-col h-full w-full bg-dark-900 select-none overflow-hidden" onClick={() => setMenu(null)}>
      {/* 1. Header with Controls (Mimics CredentialsWidget.cpp) */}
      <div className="flex items-center justify-between px-4 py-2 bg-dark-800 border-b border-dark-700 shrink-0">
        <div className="flex items-center space-x-3">
          <div className="flex items-center space-x-2 px-2 py-0.5 rounded bg-accent-primary/10 border border-accent-primary/20">
            <Key className="w-3.5 h-3.5 text-accent-primary" />
            <span className="text-[10px] font-black uppercase tracking-widest text-accent-primary">Credentials</span>
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
            <span className="text-[10px] font-bold uppercase">Add Cred</span>
          </button>
        </div>
      </div>

      {/* 2. Search Panel */}
      {isSearchVisible && (
        <div className="flex items-center px-4 py-2 bg-dark-800/50 border-b border-dark-700 animate-in slide-in-from-top-2 duration-200 shrink-0 space-x-4">
          <div className="relative flex-1 max-w-md">
            <Filter className="w-3 h-3 absolute left-2.5 top-1/2 -translate-y-1/2 text-gray-600" />
            <input 
              type="text" 
              autoFocus
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="filter: (adm | user) & hash..." 
              className="w-full bg-dark-950/50 border border-dark-600 rounded px-8 py-1 text-[11px] text-gray-300 outline-none focus:border-accent-primary/50"
            />
          </div>
          <select 
            value={filterType}
            onChange={(e) => setFilterType(e.target.value)}
            className="bg-dark-950/50 border border-dark-600 rounded px-2 py-1 text-[11px] text-gray-300 outline-none focus:border-accent-primary/50"
          >
            {credTypes.map(t => <option key={t} value={t}>{t}</option>)}
          </select>
        </div>
      )}

      {/* 3. Table Area */}
      <div className="flex-1 overflow-auto scrollbar-thin">
        <table className="w-full text-left border-collapse table-auto min-w-[1000px]">
          <thead className="sticky top-0 bg-dark-800 z-10 shadow-sm">
            <tr className="border-b border-dark-700 text-gray-500 text-[10px] font-bold uppercase tracking-tight">
              <th className="py-2 px-4 border-r border-dark-700/30">User</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Password / Hash</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Realm / Domain</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Type</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Source</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Tag</th>
              <th className="py-2 px-4 text-right">Actions</th>
            </tr>
          </thead>
          <tbody className="text-[11px] font-medium divide-y divide-dark-800/30">
            {filteredCreds.length === 0 ? (
              <tr>
                <td colSpan="7" className="py-20 text-center text-gray-600 italic">
                  <div className="flex flex-col items-center space-y-3 opacity-20">
                    <Key size={40} />
                    <p className="text-xs font-medium tracking-widest uppercase">No credentials looted</p>
                  </div>
                </td>
              </tr>
            ) : (
              filteredCreds.map((c) => (
                <tr 
                  key={c.cred_id} 
                  className="hover:bg-accent-primary/5 transition-colors group h-8 cursor-default"
                >
                  <td className="px-4 text-accent-primary font-bold truncate flex items-center space-x-2">
                    <User size={12} className="text-gray-500" />
                    <span>{c.username}</span>
                  </td>
                  <td className="px-4 text-gray-300 font-mono truncate max-w-xs select-text" onClick={() => handleCopyPassword(c.password)}>
                    {c.password}
                  </td>
                  <td className="px-4 text-gray-400 font-mono truncate">{c.realm || '---'}</td>
                  <td className="px-4">
                    <span className="px-1.5 py-0.5 rounded bg-dark-700 text-[9px] font-black uppercase text-gray-400 border border-dark-600">
                      {c.type}
                    </span>
                  </td>
                  <td className="px-4 text-gray-500 text-[10px] truncate">
                    <div className="flex items-center space-x-1">
                      <Database size={10} />
                      <span>{c.host || c.agent_id?.substring(0,8)}</span>
                    </div>
                  </td>
                  <td className="px-4">
                    {c.tag && (
                      <span className="px-1.5 py-0.5 rounded bg-accent-secondary/10 text-[9px] font-black uppercase text-accent-secondary border border-accent-secondary/20">
                        {c.tag}
                      </span>
                    )}
                  </td>
                  <td className="px-4 text-right">
                    <div className="flex items-center justify-end space-x-1 opacity-0 group-hover:opacity-100 transition-opacity">
                      <button onClick={() => handleCopyPassword(c.password)} className="p-1 rounded hover:bg-dark-700 text-gray-400 hover:text-white transition-colors" title="Copy Password">
                        <Copy size={14} />
                      </button>
                      <button 
                        onClick={() => handleEdit(c)}
                        className="p-1 rounded hover:bg-dark-700 text-gray-400 hover:text-white transition-colors" 
                        title="Edit"
                      >
                        <Edit3 size={14} />
                      </button>
                      <button 
                        onClick={() => handleRemove(c.cred_id)}
                        className="p-1 rounded hover:bg-dark-700 text-accent-danger transition-colors" 
                        title="Delete"
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
          <span>Total Credentials: <span className="text-accent-primary">{credentials.length}</span></span>
        </div>
        <div className="flex items-center space-x-1">
          <Shield size={10} className="text-accent-secondary" />
          <span className="text-accent-secondary/80">Encrypted Loot Sync</span>
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
