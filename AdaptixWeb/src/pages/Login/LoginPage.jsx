import React, { useState, useEffect, useCallback } from 'react';
import { ShieldAlert, Lock, User, Globe, Server, Save, Trash2, Plus, Terminal, Activity, Link2, X } from 'lucide-react';
import axios from 'axios';
import { cn } from '../../utils/cn';
import { useConfig } from '../../context/ConfigContext';

const LoginPage = ({ onLogin }) => {
  const { config, updateConfigFromUrl } = useConfig();
  const [profiles, setProfiles] = useState(() => {
    const saved = localStorage.getItem('adaptix_profiles');
    return saved ? JSON.parse(saved) : [];
  });
  
  const [selectedProfileId, setSelectedProfileId] = useState(null);
  const [formData, setFormData] = useState({
    name: 'Default',
    url: config.teamserverUrl,
    username: '',
    password: '',
  });
  
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');

  // Computed endpoint for display
  const [derivedEndpoint, setDerivedEndpoint] = useState(config.apiEndpoint);

  // Use useCallback to prevent infinite loop in useEffect
  const memoizedUpdateConfig = useCallback((url) => {
    return updateConfigFromUrl(url);
  }, [updateConfigFromUrl]);

  useEffect(() => {
    // Update derived endpoint whenever formData.url changes
    const newConfig = memoizedUpdateConfig(formData.url);
    if (newConfig) {
      setDerivedEndpoint(newConfig.apiEndpoint);
    }
  }, [formData.url, memoizedUpdateConfig]);

  useEffect(() => {
    localStorage.setItem('adaptix_profiles', JSON.stringify(profiles));
  }, [profiles]);

  const handleUrlChange = (val) => {
    setFormData(prev => ({ ...prev, url: val }));
    updateConfigFromUrl(val);
  };

  const handleProfileSelect = (profile) => {
    setFormData(profile);
    setSelectedProfileId(profile.id);
    updateConfigFromUrl(profile.url);
  };

  const handleSaveProfile = () => {
    if (!formData.url || !formData.username) return;
    
    const profileToSave = {
      name: formData.name,
      url: formData.url,
      username: formData.username,
      password: formData.password,
      id: selectedProfileId || Date.now().toString(),
    };

    if (selectedProfileId) {
      setProfiles(prev => prev.map(p => p.id === selectedProfileId ? profileToSave : p));
    } else {
      setProfiles(prev => [...prev, profileToSave]);
      setSelectedProfileId(profileToSave.id);
    }
  };

  const handleDeleteProfile = (id, e) => {
    e.stopPropagation();
    setProfiles(prev => prev.filter(p => p.id !== id));
    if (selectedProfileId === id) {
      setSelectedProfileId(null);
    }
  };

  const handleLogin = async (e) => {
    if (e) e.preventDefault();
    setLoading(true);
    setError('');
    
    try {
      // Force config update from current URL to ensure we have the latest path immediately
      // This returns the new config object directly, bypassing state update delay
      const currentConfig = updateConfigFromUrl(formData.url);
      const apiEndpoint = currentConfig ? currentConfig.apiEndpoint : config.apiEndpoint;
      
      console.log('[Login] Target URL:', formData.url);
      console.log('[Login] Derived API Endpoint:', apiEndpoint);
      
      // Use relative path to trigger Vite proxy and avoid CORS issues
      // Dynamically use the configured apiEndpoint (which includes /api prefix for local)
      const loginUrl = `${apiEndpoint}/login`;
      console.log('[Login] Final Request URL:', loginUrl);

      const response = await axios.post(loginUrl, {
        username: formData.username,
        password: formData.password,
        version: 'v1.0'
      });
      
      if (response.data && response.data.access_token) {
        localStorage.setItem('adaptix_url', formData.url);
        localStorage.setItem('adaptix_user', formData.username);
        localStorage.setItem('adaptix_token', response.data.access_token);
        localStorage.setItem('adaptix_refresh_token', response.data.refresh_token);
        onLogin(true);
      } else {
        setError('Invalid response from server');
      }
    } catch (err) {
      console.error('Login error:', err);
      if (err.response) {
        setError(err.response.data?.message || `Auth failed: ${err.response.status}`);
      } else if (err.request) {
        setError('Connection refused. Verify Teamserver is running and reachable.');
      } else {
        setError('Request failed. Check console for details.');
      }
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="fixed inset-0 bg-dark-950 flex items-center justify-center overflow-hidden font-sans text-[#BEBEBE] select-none">
      {/* Background Decor */}
      <div className="absolute inset-0 overflow-hidden pointer-events-none opacity-10">
        <div className="absolute top-0 left-0 w-full h-full bg-[linear-gradient(rgba(61,139,106,0.05)_1px,transparent_1px),linear-gradient(90deg,rgba(61,139,106,0.05)_1px,transparent_1px)] bg-[size:30px_30px]"></div>
      </div>

      <div className="relative w-full max-w-4xl grid grid-cols-1 md:grid-cols-5 gap-0 bg-dark-900 border border-dark-700 shadow-[0_0_60px_rgba(0,0,0,0.8)] rounded-sm overflow-hidden z-10 mx-4">
        
        {/* Left Panel: Profiles */}
        <div className="md:col-span-2 bg-dark-800 border-r border-dark-700 p-6 flex flex-col">
          <div className="flex items-center space-x-3 mb-10">
            <div className="p-2 bg-accent-primary/10 rounded-sm border border-accent-primary/20">
              <ShieldAlert className="w-8 h-8 text-accent-primary" />
            </div>
            <div>
              <h1 className="text-lg font-black text-white tracking-widest uppercase">Adaptix C2</h1>
              <div className="flex items-center space-x-2">
                <div className="w-1.5 h-1.5 rounded-full bg-accent-secondary animate-pulse"></div>
                <span className="text-[9px] text-gray-500 font-black uppercase tracking-widest">v1.0.0-STABLE</span>
              </div>
            </div>
          </div>

          <div className="flex-1 flex flex-col min-h-0">
            <div className="flex items-center justify-between mb-3 px-1">
              <h3 className="text-[9px] font-black text-gray-500 uppercase tracking-widest flex items-center">
                <Server className="w-3 h-3 mr-2" />
                Connectivity Profiles
              </h3>
              <button 
                onClick={() => {
                  setSelectedProfileId(null);
                  setFormData({ name: 'New Profile', url: config.teamserverUrl, username: '', password: '' });
                }}
                className="p-1 hover:bg-dark-700 rounded transition-colors text-gray-500 hover:text-accent-primary"
                title="Create New Profile"
              >
                <Plus size={14} />
              </button>
            </div>
            
            <div className="flex-1 space-y-1 overflow-y-auto scrollbar-thin pr-1 min-h-0">
              {profiles.length === 0 ? (
                <div className="text-[10px] text-gray-600 italic py-8 text-center border border-dashed border-dark-700 rounded-sm bg-dark-950/30">
                  No saved configurations
                </div>
              ) : (
                profiles.map(profile => (
                  <div 
                    key={profile.id}
                    onClick={() => handleProfileSelect(profile)}
                    className={cn(
                      "group flex items-center justify-between p-2.5 rounded-sm border transition-all cursor-default",
                      selectedProfileId === profile.id 
                        ? "bg-accent-selection/30 border-accent-primary text-white shadow-inner" 
                        : "bg-dark-950/50 border-dark-700 text-gray-500 hover:border-dark-600 hover:bg-dark-700/30"
                    )}
                  >
                    <div className="flex flex-col min-w-0">
                      <span className="text-[11px] font-bold truncate uppercase tracking-tight">{profile.name}</span>
                      <span className="text-[9px] font-mono text-gray-600 truncate">{profile.url}</span>
                    </div>
                    <button 
                      onClick={(e) => handleDeleteProfile(profile.id, e)}
                      className="opacity-0 group-hover:opacity-100 p-1 text-gray-600 hover:text-accent-danger transition-all"
                    >
                      <X size={12} />
                    </button>
                  </div>
                ))
              )}
            </div>

            <div className="mt-4 pt-4 border-t border-dark-700">
              <div className="flex items-center space-x-1 text-[9px] font-black uppercase text-gray-600 tracking-widest">
                <Activity size={10} className="text-accent-secondary opacity-50" />
                <span>Infrastructure verified</span>
              </div>
            </div>
          </div>
        </div>

        {/* Right Panel: Login Form */}
        <div className="md:col-span-3 p-10 flex flex-col justify-center bg-dark-900">
          <div className="mb-8 border-l-4 border-accent-primary pl-6">
            <h2 className="text-xl font-black text-white uppercase tracking-wider">Access Terminal</h2>
            <p className="text-[11px] text-gray-500 mt-1 uppercase font-bold tracking-tight">Enter credentials to establish teamserver link</p>
          </div>

          <form className="space-y-4" onSubmit={handleLogin}>
            <div className="space-y-4">
              <div className="grid grid-cols-3 gap-3">
                <div className="col-span-1 space-y-1.5">
                  <label className="text-[9px] font-black uppercase text-gray-500 tracking-widest ml-1">Alias</label>
                  <input
                    type="text"
                    className="qt-input w-full py-2.5"
                    placeholder="Profile name"
                    value={formData.name}
                    onChange={(e) => setFormData({ ...formData, name: e.target.value })}
                  />
                </div>
                <div className="col-span-2 space-y-1.5">
                  <label className="text-[9px] font-black uppercase text-gray-500 tracking-widest ml-1">Remote Host</label>
                  <div className="relative">
                    <div className="absolute inset-y-0 left-0 pl-3 flex items-center pointer-events-none">
                      <Globe className="h-3.5 w-3.5 text-gray-600" />
                    </div>
                    <input
                      type="text"
                      required
                      className="qt-input w-full pl-9 py-2.5"
                      placeholder="https://127.0.0.1:443"
                      value={formData.url}
                      onChange={(e) => handleUrlChange(e.target.value)}
                    />
                  </div>
                  <div className="text-[9px] font-mono text-gray-500 pl-1 mt-1">
                     Target API: <span className="text-accent-secondary">{derivedEndpoint}/login</span>
                  </div>
                </div>
              </div>

              <div className="bg-dark-950 border border-dark-700 rounded-sm p-3 space-y-2 shadow-inner">
                <div className="flex items-center justify-between text-[8px] font-mono text-gray-600 uppercase tracking-[0.2em] border-b border-dark-700/50 pb-1.5 mb-1.5">
                  <div className="flex items-center">
                    <Link2 size={10} className="mr-1.5 text-accent-primary" />
                    Internal Mapping
                  </div>
                  <span className="text-accent-secondary opacity-50">AUTOMATED</span>
                </div>
                <div className="grid grid-cols-2 gap-4">
                  <div className="min-w-0">
                    <span className="block text-[8px] text-gray-700 mb-0.5 uppercase font-black">Rest Interface</span>
                    <span className="block text-[9px] font-mono text-gray-400 truncate">
                      {config.apiEndpoint}
                    </span>
                  </div>
                  <div className="min-w-0">
                    <span className="block text-[8px] text-gray-700 mb-0.5 uppercase font-black">Socket stream</span>
                    <span className="block text-[9px] font-mono text-gray-400 truncate">
                      {config.wsEndpoint}
                    </span>
                  </div>
                </div>
              </div>

              <div className="space-y-1.5">
                <label className="text-[9px] font-black uppercase text-gray-500 tracking-widest ml-1">Operator ID</label>
                <div className="relative">
                  <div className="absolute inset-y-0 left-0 pl-3 flex items-center pointer-events-none">
                    <User className="h-3.5 w-3.5 text-gray-600" />
                  </div>
                  <input
                    type="text"
                    required
                    className="qt-input w-full pl-9 py-2.5 font-mono"
                    placeholder="Username"
                    value={formData.username}
                    onChange={(e) => setFormData({ ...formData, username: e.target.value })}
                  />
                </div>
              </div>

              <div className="space-y-1.5">
                <label className="text-[9px] font-black uppercase text-gray-500 tracking-widest ml-1">Secret Key</label>
                <div className="relative">
                  <div className="absolute inset-y-0 left-0 pl-3 flex items-center pointer-events-none">
                    <Lock className="h-3.5 w-3.5 text-gray-600" />
                  </div>
                  <input
                    type="password"
                    required
                    autoComplete="current-password"
                    className="qt-input w-full pl-9 py-2.5 font-mono"
                    placeholder="••••••••"
                    value={formData.password}
                    onChange={(e) => setFormData({ ...formData, password: e.target.value })}
                  />
                </div>
              </div>
            </div>

            {error && (
              <div className="text-accent-danger text-[10px] bg-accent-danger/5 p-2.5 rounded-sm border border-accent-danger/20 flex items-start space-x-2 animate-in slide-in-from-top-1 duration-200">
                <ShieldAlert size={12} className="shrink-0 mt-0.5" />
                <span className="uppercase font-bold tracking-tight">{error}</span>
              </div>
            )}

            <div className="pt-4">
              <button
                type="submit"
                disabled={loading}
                className="w-full qt-btn bg-[#2a2a2a] border-accent-primary/50 text-white py-3 font-black uppercase tracking-[0.2em] shadow-lg shadow-accent-primary/10 hover:bg-accent-selection active:scale-[0.99] transition-all disabled:opacity-50"
              >
                {loading ? (
                  <div className="w-4 h-4 border-2 border-white/30 border-t-white rounded-full animate-spin mx-auto" />
                ) : (
                  'Establish Connection'
                )}
              </button>
            </div>
          </form>
        </div>
      </div>
      
      <div className="absolute bottom-8 left-0 w-full text-center pointer-events-none">
        <p className="text-[10px] text-gray-700 font-mono tracking-widest uppercase">
          &copy; 2026 Adaptix Intelligence Security Framework | Authorized Access Only
        </p>
      </div>
    </div>
  );
};

export default LoginPage;
