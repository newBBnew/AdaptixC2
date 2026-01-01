import React, { useState, useEffect } from 'react';
import { 
  Database, 
  Search, 
  Filter, 
  Plus, 
  Trash2, 
  Link, 
  Copy,
  RefreshCw,
  X,
  FileUp,
  Download
} from 'lucide-react';
import { deliveryApi } from '../../api/control';
import { cn } from '../../utils/cn';

const FileDeliveryList = () => {
  const [files, setFiles] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [isSearchVisible, setIsSearchVisible] = useState(false);

  const fetchFiles = async () => {
    try {
      setLoading(true);
      const response = await deliveryApi.list();
      setFiles(Array.isArray(response.data) ? response.data : []);
      setError(null);
    } catch (err) {
      console.error('Failed to fetch hosted files:', err);
      setError('Connection failed');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchFiles();
    const interval = setInterval(fetchFiles, 30000);
    return () => clearInterval(interval);
  }, []);

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

  const filteredFiles = files.filter(f => 
    Object.values(f).some(val => 
      String(val).toLowerCase().includes(searchQuery.toLowerCase())
    )
  );

  const handleDeleteFile = async (id) => {
    if (!window.confirm('Are you sure you want to delete this hosted file?')) return;
    try {
      await deliveryApi.stop(id);
      fetchFiles();
    } catch (err) {
      console.error('Failed to delete file:', err);
    }
  };

  const handleCopyUrl = (url) => {
    navigator.clipboard.writeText(url);
    // Could add a toast here
  };

  return (
    <div className="flex flex-col h-full bg-dark-900 text-gray-300 font-sans select-none overflow-hidden">
      {/* 1. Header with Controls (Mimics FileDeliveryWidget.cpp) */}
      <div className="flex items-center justify-between px-3 py-1.5 bg-dark-800 border-b border-dark-700 shrink-0">
        <div className="flex items-center space-x-3">
          <div className="flex items-center space-x-2 px-2 py-0.5 rounded bg-accent-primary/10 border border-accent-primary/20">
            <Database className="w-3.5 h-3.5 text-accent-primary" />
            <span className="text-[10px] font-black uppercase tracking-widest text-accent-primary">File Delivery</span>
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
            onClick={fetchFiles}
            className="p-1.5 rounded hover:bg-dark-700 text-gray-400 hover:text-white transition-all"
            title="Refresh"
          >
            <RefreshCw className={cn("w-3.5 h-3.5", loading && "animate-spin text-accent-primary")} />
          </button>
          <div className="h-4 w-px bg-dark-600 mx-1" />
          <button className="flex items-center space-x-1.5 px-3 py-1 rounded bg-accent-primary/10 border border-accent-primary/30 text-accent-primary hover:bg-accent-primary/20 transition-all group">
            <FileUp className="w-3.5 h-3.5 group-hover:scale-110 transition-transform" />
            <span className="text-[10px] font-bold uppercase">Upload</span>
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
              placeholder="Filter files..." 
              className="w-full bg-dark-950/50 border border-dark-600 rounded px-8 py-1 text-[11px] text-gray-300 outline-none focus:border-accent-primary/50"
            />
          </div>
        </div>
      )}

      {/* 3. Table Area */}
      <div className="flex-1 overflow-auto scrollbar-thin">
        <table className="w-full text-left border-collapse table-auto min-w-[800px]">
          <thead className="sticky top-0 bg-dark-800 z-10 shadow-sm">
            <tr className="border-b border-dark-700 text-gray-500 text-[10px] font-bold uppercase tracking-tight">
              <th className="py-2 px-4 border-r border-dark-700/30">Filename</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Size</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Downloads</th>
              <th className="py-2 px-4 border-r border-dark-700/30">URL</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Date Added</th>
              <th className="py-2 px-4 text-right">Actions</th>
            </tr>
          </thead>
          <tbody className="text-[11px] font-medium divide-y divide-dark-800/30">
            {filteredFiles.length === 0 ? (
              <tr>
                <td colSpan="6" className="py-20 text-center text-gray-600 italic">
                  <div className="flex flex-col items-center space-y-3 opacity-20">
                    <Database size={40} />
                    <p className="text-xs font-medium tracking-widest uppercase">No hosted files</p>
                  </div>
                </td>
              </tr>
            ) : (
              filteredFiles.map((f) => (
                <tr 
                  key={f.id} 
                  className="hover:bg-accent-primary/5 transition-colors group h-8 cursor-default"
                >
                  <td className="px-4 text-accent-primary font-bold font-mono truncate">{f.filename}</td>
                  <td className="px-4 text-gray-300 font-mono truncate">{f.size || '---'}</td>
                  <td className="px-4 text-gray-300 font-mono text-center w-24">
                    <span className="px-1.5 py-0.5 rounded bg-dark-700 text-[9px] font-black text-accent-secondary">
                      {f.downloads || 0}
                    </span>
                  </td>
                  <td className="px-4 text-gray-400 font-mono truncate max-w-xs select-text hover:text-white transition-colors cursor-pointer" onClick={() => handleCopyUrl(f.url)}>
                    {f.url || '---'}
                  </td>
                  <td className="px-4 text-gray-500 font-mono truncate">
                    {f.date ? new Date(f.date * 1000).toLocaleString() : '---'}
                  </td>
                  <td className="px-4 text-right">
                    <div className="flex items-center justify-end space-x-1 opacity-0 group-hover:opacity-100 transition-opacity">
                      <button 
                        onClick={() => handleCopyUrl(f.url)}
                        className="p-1 rounded hover:bg-dark-700 text-gray-400 hover:text-white transition-colors" 
                        title="Copy URL"
                      >
                        <Copy size={14} />
                      </button>
                      <button 
                        className="p-1 rounded hover:bg-dark-700 text-gray-400 hover:text-accent-secondary transition-colors" 
                        title="Create Link"
                      >
                        <Link size={14} />
                      </button>
                      <button 
                        onClick={() => handleDeleteFile(f.id)}
                        className="p-1 rounded hover:bg-dark-700 text-accent-danger transition-colors" 
                        title="Delete File"
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
          <span>Hosted Files: <span className="text-accent-primary">{files.length}</span></span>
        </div>
        <div className="flex items-center space-x-1">
          <div className="w-1.5 h-1.5 rounded-full bg-accent-secondary animate-pulse" />
          <span className="text-accent-secondary/80">File Delivery Sync Active</span>
        </div>
      </div>
    </div>
  );
};

export default FileDeliveryList;
