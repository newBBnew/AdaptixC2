import React, { useState, useEffect, useCallback } from 'react';
import { ShieldAlert, Lock, User, Globe, Server, Save, Trash2, Plus, Terminal, Activity, Link2, X, RefreshCw } from 'lucide-react';
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
  const [derivedEndpoint, setDerivedEndpoint] = useState(config.apiEndpoint);

  const memoizedUpdateConfig = useCallback((url) => {
    return updateConfigFromUrl(url);
  }, [updateConfigFromUrl]);

  useEffect(() => {
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
      const currentConfig = updateConfigFromUrl(formData.url);
      const apiEndpoint = currentConfig ? currentConfig.apiEndpoint : config.apiEndpoint;
      const loginUrl = `${apiEndpoint}/login`;
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
      if (err.response) {
        setError(err.response.data?.message || `Auth failed: ${err.response.status}`);
      } else if (err.request) {
        setError('Connection refused. Verify the service is running and reachable.');
      } else {
        setError('Request failed. Check console for details.');
      }
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="min-h-screen w-full flex items-center justify-center p-4 overflow-hidden relative">
      <div className="absolute inset-0 overflow-hidden pointer-events-none z-0">
        <div className="absolute top-[-10%] left-[-10%] w-[40%] h-[40%] rounded-full bg-theme-accent/10 blur-[120px] animate-pulse" />
        <div className="absolute bottom-[-10%] right-[-10%] w-[40%] h-[40%] rounded-full bg-theme-accent-secondary/10 blur-[120px] animate-pulse delay-700" />
      </div>

      <div className="w-full max-w-5xl grid grid-cols-1 lg:grid-cols-12 gap-8 relative z-10 text-left">
        <div className="lg:col-span-5 flex flex-col justify-center space-y-8 p-4">
          <div className="space-y-4">
            <div className="inline-flex items-center space-x-3 px-4 py-2 glass-card-sm border-theme-glass-light rounded-2xl shadow-glow-sm">
              <ShieldAlert className="w-6 h-6 text-theme-accent" />
              <span className="text-xs font-black uppercase tracking-[0.3em] text-theme-primary">Operations Console</span>
            </div>
            <h1 className="text-7xl font-black tracking-tighter text-theme-primary">
              OPS <span className="gradient-text">CONSOLE</span>
            </h1>
            <p className="text-lg text-theme-muted font-bold max-w-md leading-relaxed">
              Secure command orchestration and infrastructure coordination for field operations.
            </p>
          </div>

          <div className="grid grid-cols-2 gap-4">
            {[
              { icon: Activity, label: 'Real-time', desc: 'Sync telemetry' },
              { icon: Terminal, label: 'Orchestration', desc: 'Advanced canvas' }
            ].map((item, i) => (
              <div key={i} className="glass-panel p-4 rounded-2xl border-theme-glass-light hover:border-theme-accent transition-all group">
                <item.icon className="w-5 h-5 text-theme-accent mb-3 group-hover:scale-110 transition-transform" />
                <p className="text-xs font-black uppercase tracking-widest text-theme-primary">{item.label}</p>
                <p className="text-[10px] text-theme-muted font-bold mt-1 uppercase">{item.desc}</p>
              </div>
            ))}
          </div>
        </div>

        <div className="lg:col-span-7">
          <div className="glass-card p-1 rounded-[2.5rem] shadow-2xl border-theme-glass-light">
            <div className="bg-theme-glass-panel rounded-[2.2rem] p-8 md:p-12">
              <div className="flex items-center justify-between mb-10 text-left">
                <div>
                  <h2 className="text-2xl font-black text-theme-primary tracking-tight uppercase">Control Node Access</h2>
                  <p className="text-sm text-theme-muted font-bold uppercase mt-1">Authorized Access Only</p>
                </div>
                <div className="glass-btn p-3 rounded-2xl border-theme-glass-light">
                  <Lock className="w-6 h-6 text-theme-accent" />
                </div>
              </div>

              <form onSubmit={handleLogin} className="space-y-6">
                <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                  <div className="space-y-2 md:col-span-2 text-left">
                    <label className="flex items-center space-x-2 text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">
                      <Globe size={12} className="text-theme-accent" />
                      <span>Service Endpoint</span>
                    </label>
                    <div className="relative group">
                      <input 
                        type="text"
                        value={formData.url}
                        onChange={(e) => handleUrlChange(e.target.value)}
                        className="w-full glass-input pl-12 pr-4 py-4 text-theme-primary font-bold rounded-2xl focus:ring-2 focus:ring-theme-accent/30 transition-all border-theme-glass-light"
                        placeholder="https://service.local:8443"
                        required
                      />
                      <Server className="absolute left-4 top-1/2 -translate-y-1/2 text-theme-muted group-focus-within:text-theme-accent transition-colors" size={18} />
                    </div>
                    {derivedEndpoint && (derivedEndpoint.includes('localhost') || derivedEndpoint.includes('127.0.0.1')) && (
                      <p className="text-[9px] text-theme-accent-secondary font-black uppercase tracking-wider ml-1 animate-pulse">
                        Auto-detected: {derivedEndpoint}
                      </p>
                    )}
                  </div>

                  <div className="space-y-2 text-left">
                    <label className="flex items-center space-x-2 text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">
                      <User size={12} className="text-theme-accent" />
                      <span>User ID</span>
                    </label>
                    <div className="relative group">
                      <input 
                        type="text"
                        value={formData.username}
                        onChange={(e) => setFormData({...formData, username: e.target.value})}
                        className="w-full glass-input pl-12 pr-4 py-4 text-theme-primary font-bold rounded-2xl border-theme-glass-light focus:ring-2 focus:ring-theme-accent/30 transition-all"
                        placeholder="OPERATOR"
                        required
                      />
                      <User className="absolute left-4 top-1/2 -translate-y-1/2 text-theme-muted group-focus-within:text-theme-accent transition-colors" size={18} />
                    </div>
                  </div>

                  <div className="space-y-2 text-left">
                    <label className="flex items-center space-x-2 text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">
                      <Lock size={12} className="text-theme-accent" />
                      <span>Access Key</span>
                    </label>
                    <div className="relative group">
                      <input 
                        type="password"
                        value={formData.password}
                        onChange={(e) => setFormData({...formData, password: e.target.value})}
                        className="w-full glass-input pl-12 pr-4 py-4 text-theme-primary font-bold rounded-2xl border-theme-glass-light focus:ring-2 focus:ring-theme-accent/30 transition-all"
                        placeholder="••••••••"
                        required
                      />
                      <Lock className="absolute left-4 top-1/2 -translate-y-1/2 text-theme-muted group-focus-within:text-theme-accent transition-colors" size={18} />
                    </div>
                  </div>
                </div>

                {error && (
                  <div className="p-4 bg-theme-danger/10 border border-theme-danger/20 rounded-2xl flex items-center space-x-3">
                    <ShieldAlert className="text-theme-danger shrink-0" size={18} />
                    <p className="text-xs font-bold text-theme-danger uppercase tracking-wider">{error}</p>
                  </div>
                )}

                <div className="flex items-center space-x-4 pt-4">
                  <button
                    type="submit"
                    disabled={loading}
                    className="flex-1 glass-btn-primary py-4 rounded-2xl font-black uppercase tracking-[0.2em] text-sm shadow-glow-sm hover:shadow-glow hover:scale-[1.02] transition-all disabled:opacity-50 disabled:scale-100 flex items-center justify-center space-x-2"
                  >
                    {loading ? (
                      <RefreshCw className="w-5 h-5 animate-spin text-white" />
                    ) : (
                      <>
                        <span>Sign In</span>
                        <Link2 size={18} />
                      </>
                    )}
                  </button>
                  
                  <button
                    type="button"
                    onClick={handleSaveProfile}
                    className="p-4 glass-btn rounded-2xl border-theme-glass-light text-theme-accent hover:bg-theme-hover transition-all"
                    title="Save Profile"
                  >
                    <Save size={20} />
                  </button>
                </div>
              </form>

              {profiles.length > 0 && (
                <div className="mt-10 pt-10 border-t border-theme-glass-light text-left">
                  <div className="flex items-center justify-between mb-4">
                    <h3 className="text-[10px] font-black uppercase text-theme-muted tracking-widest ml-1">Saved Profiles</h3>
                    <Plus className="w-3 h-3 text-theme-accent cursor-pointer hover:rotate-90 transition-transform" />
                  </div>
                  <div className="grid grid-cols-1 md:grid-cols-2 gap-3">
                    {profiles.map(p => (
                      <div 
                        key={p.id}
                        onClick={() => handleProfileSelect(p)}
                        className={cn(
                          "glass-card-sm p-3 rounded-xl border flex items-center justify-between cursor-pointer transition-all hover:scale-[1.02]",
                          selectedProfileId === p.id ? "bg-theme-glass border-theme-accent" : "border-theme-glass-light hover:bg-theme-hover"
                        )}
                      >
                        <div className="flex items-center space-x-3 min-w-0">
                          <div className="w-8 h-8 rounded-lg bg-theme-glass flex items-center justify-center shrink-0 border border-theme-glass-light">
                            <Server size={14} className={selectedProfileId === p.id ? "text-theme-accent" : "text-theme-muted"} />
                          </div>
                          <div className="min-w-0">
                            <p className="text-[11px] font-black text-theme-primary uppercase truncate">{p.name || 'UNNAMED'}</p>
                            <p className="text-[9px] text-theme-muted font-bold truncate tracking-tighter">{p.url}</p>
                          </div>
                        </div>
                        <button 
                          onClick={(e) => handleDeleteProfile(p.id, e)}
                          className="p-1.5 hover:bg-theme-danger/10 text-theme-muted hover:text-theme-danger rounded-lg transition-colors"
                        >
                          <X size={12} strokeWidth={3} />
                        </button>
                      </div>
                    ))}
                  </div>
                </div>
              )}
            </div>
          </div>
        </div>
      </div>
      
      <div className="absolute bottom-6 left-0 w-full text-center pointer-events-none">
        <p className="text-[11px] text-theme-muted font-mono tracking-wider">
          &copy; 2026 Operations Console
        </p>
      </div>
    </div>
  );
};

export default LoginPage;
