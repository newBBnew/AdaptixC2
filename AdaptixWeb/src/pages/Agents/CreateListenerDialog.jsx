import React, { useState, useEffect } from 'react';
import Modal from '../../components/Modal';
import { cn } from '../../utils/cn';
import { processDynamicConfig } from '../../utils/configUtils';
import { listenerApi } from '../../api/control';
import { useAgents } from '../../context/AgentContext';
import DynamicFormRenderer from '../../components/DynamicFormRenderer';
import { 
  Globe, 
  Shield, 
  Settings, 
  Save, 
  Plus,
  ChevronRight,
  ChevronDown,
  Info,
  Check,
  Activity,
  Key,
  RefreshCw
} from 'lucide-react';

const CreateListenerDialog = ({ isOpen, onClose, onCreated, editMode = false, initialData = null }) => {
  const { availableListeners } = useAgents();
  const [activeTab, setActiveTab] = useState('main'); // 'main', 'headers', 'pages', 'extra'
  const [listenerName, setListenerName] = useState('');
  const [selectedType, setSelectedType] = useState('BeaconHTTP');
  const [profileName, setProfileName] = useState('');
  const [saveAsProfile, setSaveAsProfile] = useState(true);

  const getIcon = (type) => {
    if (type.toLowerCase().includes('http')) return Globe;
    if (type.toLowerCase().includes('doh')) return Shield;
    if (type.toLowerCase().includes('tcp')) return Settings;
    if (type.toLowerCase().includes('smb')) return Settings;
    return Activity;
  };

  // Merge available listeners with hardcoded fallbacks if needed, or rely on availableListeners if populated
  // The context populates availableListeners from LISTENER_REG packets.
  // We should prefer those. If empty (initially), maybe keep fallbacks?
  // Actually, 'availableListeners' from context might contain the hardcoded types IF the server sends REG packets for them.
  // Assuming Server sends REG packets for all supported listeners (including core ones).
  // If not, we merge.
  
  const coreListeners = [
    { id: 'BeaconHTTP', label: 'HTTP Beacon', icon: Globe, protocol: 'http' },
    { id: 'BeaconDoH', label: 'DoH Beacon', icon: Shield, protocol: 'doh' },
    { id: 'BeaconTCP', label: 'TCP Beacon', icon: Settings, protocol: 'bind_tcp' },
    { id: 'BeaconSMB', label: 'SMB Beacon', icon: Settings, protocol: 'bind_smb' },
  ];

  // Merge available listeners with core listeners
  // Priority: Dynamic (available) > Core (Hardcoded)
  const displayListeners = React.useMemo(() => {
    const merged = [...coreListeners];
    
    availableListeners.forEach(dynamic => {
      const index = merged.findIndex(core => core.id === dynamic.id);
      const dynamicItem = {
        id: dynamic.id,
        label: dynamic.label,
        icon: getIcon(dynamic.id),
        protocol: dynamic.protocol,
        ui_schema: dynamic.ui_schema
      };

      if (index !== -1) {
        // Update existing core listener with dynamic data
        merged[index] = { ...merged[index], ...dynamicItem };
      } else {
        // Add new dynamic listener
        merged.push(dynamicItem);
      }
    });
    
    return merged;
  }, [availableListeners]);
  
  const currentListener = displayListeners.find(l => l.id === selectedType);
  const isDynamic = !!currentListener?.ui_schema;

  // Consolidated config state
  const [config, setConfig] = useState({
    // Common
    encrypt_key: Math.random().toString(36).substring(2, 15) + Math.random().toString(36).substring(2, 15),
    
    // HTTP & DoH Common
    bind_host: '0.0.0.0',
    bind_port: '443',
    user_agent: 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
    
    // HTTP Specific
    agent_host: '', // callback_addresses
    http_method: 'POST',
    uri: '/api/v1/update',
    hb_header: 'X-Session-Token', // parameter_name
    proxy: '',
    header: '', // request_headers
    host_header: '',
    trust_x_forwarded: false,
    server_headers: 'Server: nginx\nContent-Type: application/json',
    page_error: '<!DOCTYPE html><html><body><h1>404 Not Found</h1></body></html>',
    page_payload: '{"status":"ok","data":"<<<PAYLOAD_DATA>>>"}',
    ssl: false,
    
    // DoH Specific
    domain: '',
    doh_urls: '8.8.8.8', // resolver
    
    // SMB Specific
    pipe_name: 'adaptix_pipe',
    
    // TCP Specific
    bind_port_tcp: '4444'
  });

  useEffect(() => {
    if (initialData) {
      setListenerName(initialData.l_name || '');
      // Use l_reg_name (e.g. BeaconHTTP) for selectedType, fallback to l_type only if reg_name missing (unlikely for sync packet)
      setSelectedType(initialData.l_reg_name || initialData.l_type || 'BeaconHTTP');
      try {
        const parsedConfig = typeof initialData.l_config === 'string' 
          ? JSON.parse(initialData.l_config) 
          : initialData.l_config;
        
        // Map backend fields back to frontend state if needed
        if (parsedConfig) {
          setConfig(prev => ({
            ...prev,
            ...parsedConfig,
            agent_host: parsedConfig.callback_addresses || prev.agent_host,
            bind_host: parsedConfig.host_bind || prev.bind_host,
            bind_port: parsedConfig.port_bind || prev.bind_port,
            hb_header: parsedConfig.parameter_name || parsedConfig.hb_header || prev.hb_header,
            header: parsedConfig.request_headers || prev.header,
            server_headers: typeof parsedConfig.response_headers === 'object' 
              ? Object.entries(parsedConfig.response_headers).map(([k,v]) => `${k}: ${v}`).join('\n')
              : (parsedConfig.server_headers || prev.server_headers),
            page_error: parsedConfig.web_page_error || parsedConfig['page-error'] || prev.page_error,
            page_payload: parsedConfig.web_page_output || parsedConfig['page-payload'] || prev.page_payload,
            // DoH
            doh_urls: parsedConfig.doh_urls || prev.doh_urls,
            // SMB
            pipe_name: parsedConfig.pipename || prev.pipe_name,
            // TCP
            bind_port_tcp: parsedConfig.port || prev.bind_port_tcp,
            // Common
            encrypt_key: parsedConfig.encrypt_key || prev.encrypt_key
          }));
        }
      } catch (e) {
        console.error('Failed to parse listener config', e);
      }
    } else {
      setListenerName('');
      setProfileName('');
      // Reset config on new
      setConfig(prev => ({
        ...prev,
        encrypt_key: Math.random().toString(36).substring(2, 15) + Math.random().toString(36).substring(2, 15)
      }));
    }
  }, [initialData]);

  // Reset active tab when type changes
  useEffect(() => {
    setActiveTab('main');
  }, [selectedType]);

  const generateKey = () => {
    setConfig(prev => ({
      ...prev,
      encrypt_key: Array.from({length:32}, () => Math.floor(Math.random()*16).toString(16)).join('')
    }));
  };

  const handleCreate = async () => {
    if (!listenerName.trim()) {
      alert('Listener name is required');
      return;
    }

    // Prepare config object based on selected type
    let finalConfig = {
      encrypt_key: config.encrypt_key
    };

    if (isDynamic) {
      // Dynamic Listener: Use config state directly but sanitize types
      finalConfig = { 
        ...finalConfig,
        ...processDynamicConfig(config, currentListener.ui_schema) 
      };
    } else if (selectedType === 'BeaconHTTP') {
      finalConfig = {
        ...finalConfig,
        host_bind: config.bind_host,
        port_bind: parseInt(config.bind_port),
        callback_addresses: config.agent_host,
        http_method: config.http_method,
        uri: config.uri,
        user_agent: config.user_agent,
        hb_header: config.hb_header, 
        host_header: config.host_header,
        request_headers: config.header,
        server_headers: config.server_headers,
        proxy: config.proxy,
        trust_x_forwarded: config.trust_x_forwarded,
        'x-forwarded-for': config.trust_x_forwarded,
        'page-error': config.page_error,
        'page-payload': config.page_payload,
        ssl: config.ssl
      };
    } else if (selectedType === 'BeaconDoH') {
      finalConfig = {
        ...finalConfig,
        host_bind: config.bind_host,
        port_bind: parseInt(config.bind_port),
        domain: config.domain,
        doh_urls: config.doh_urls,
        user_agent: config.user_agent,
        ssl: config.ssl
      };
    } else if (selectedType === 'BeaconSMB') {
      finalConfig = {
        ...finalConfig,
        pipename: config.pipe_name 
      };
    } else if (selectedType === 'BeaconTCP') {
      finalConfig = {
        ...finalConfig,
        port: parseInt(config.bind_port_tcp)
      };
    }

    const payload = {
      name: listenerName,
      type: selectedType,
      config: JSON.stringify(finalConfig)
    };

    try {
      if (editMode) {
        await listenerApi.edit(payload);
      } else {
        await listenerApi.start(payload);
      }
      onCreated?.();
      onClose();
    } catch (err) {
      console.error('Failed to save listener:', err);
      alert(err.response?.data?.message || 'Failed to save listener');
    }
  };

  const renderHTTPFields = () => (
    <>
      {activeTab === 'main' && (
        <div className="grid grid-cols-2 gap-2 animate-in fade-in slide-in-from-top-1 duration-200">
          <div className="space-y-1 text-left">
            <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">Host & port (Bind):</label>
            <div className="flex space-x-2">
              <input 
                type="text"
                value={config.bind_host}
                onChange={(e) => setConfig({...config, bind_host: e.target.value})}
                className="glass-input flex-1 font-mono py-1.5 px-3 text-theme-primary text-xs"
              />
              <input 
                type="text"
                value={config.bind_port}
                onChange={(e) => setConfig({...config, bind_port: e.target.value})}
                className="glass-input w-20 font-mono py-1.5 px-3 text-theme-primary text-xs text-center"
              />
            </div>
          </div>

          <div className="col-span-2 space-y-1 text-left">
            <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest flex items-center ml-1">
              Callback addresses:
              <Info size={10} className="ml-2 text-theme-muted" />
            </label>
            <textarea 
              rows="2"
              value={config.agent_host}
              onChange={(e) => setConfig({...config, agent_host: e.target.value})}
              placeholder="192.168.1.1:443&#10;server2.com:8080"
              className="glass-input w-full font-mono resize-none h-16 py-2 px-3 text-theme-primary text-[10px]"
            />
          </div>

          <div className="space-y-1 text-left">
            <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">Method:</label>
            <select 
              value={config.http_method}
              onChange={(e) => setConfig({...config, http_method: e.target.value})}
              className="glass-input w-full font-mono py-1.5 px-3 text-theme-primary text-xs appearance-none"
            >
              <option value="POST" className="bg-theme-glass-panel">POST</option>
              <option value="GET" className="bg-theme-glass-panel">GET</option>
            </select>
          </div>

          <div className="space-y-1 text-left">
            <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">URI:</label>
            <input 
              type="text"
              value={config.uri}
              onChange={(e) => setConfig({...config, uri: e.target.value})}
              placeholder="/uri.php"
              className="glass-input w-full font-mono py-1.5 px-3 text-theme-primary text-xs"
            />
          </div>

          <div className="col-span-2 space-y-1 text-left">
            <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">User-Agent:</label>
            <input 
              type="text"
              value={config.user_agent}
              onChange={(e) => setConfig({...config, user_agent: e.target.value})}
              className="glass-input w-full font-mono py-1.5 px-3 text-theme-primary text-xs"
            />
          </div>

          <div className="space-y-1 text-left">
            <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">Heartbeat Header:</label>
            <input 
              type="text"
              value={config.hb_header}
              onChange={(e) => setConfig({...config, hb_header: e.target.value})}
              placeholder="X-Beacon-Id"
              className="glass-input w-full font-mono py-1.5 px-3 text-theme-primary text-xs"
            />
          </div>

          <div className="col-span-2 flex items-center space-x-3 p-2 glass-card-sm border-theme-glass-light rounded-xl">
            <input 
              type="checkbox"
              id="use_ssl"
              checked={config.ssl}
              onChange={(e) => setConfig({...config, ssl: e.target.checked})}
              className="w-3 h-3 rounded border-theme-glass text-theme-accent focus:ring-theme-accent/30"
            />
            <label htmlFor="use_ssl" className="text-[10px] font-bold text-theme-primary uppercase tracking-widest cursor-pointer">
              Use SSL (HTTPS)
            </label>
          </div>
        </div>
      )}

      {activeTab === 'headers' && (
        <div className="space-y-2 animate-in fade-in slide-in-from-top-1 duration-200">
          <div className="flex items-center space-x-3 p-3 glass-card-sm border-theme-glass-light rounded-xl text-left">
            <input 
              type="checkbox"
              id="trust_x"
              checked={config.trust_x_forwarded}
              onChange={(e) => setConfig({...config, trust_x_forwarded: e.target.checked})}
              className="w-3 h-3 rounded border-theme-glass text-theme-accent focus:ring-theme-accent/30"
            />
            <label htmlFor="trust_x" className="text-[10px] font-bold text-theme-primary uppercase tracking-widest cursor-pointer">
              Trust X-Forwarded-For
            </label>
          </div>
          <div className="space-y-1 text-left">
            <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">Host Header:</label>
            <input 
              type="text"
              value={config.host_header}
              onChange={(e) => setConfig({...config, host_header: e.target.value})}
              placeholder="www.legit-site.com"
              className="glass-input w-full font-mono py-1.5 px-3 text-theme-primary text-xs"
            />
          </div>
          <div className="grid grid-cols-2 gap-2">
            <div className="space-y-1 text-left">
              <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">Request Headers:</label>
              <textarea 
                rows="6"
                value={config.header}
                onChange={(e) => setConfig({...config, header: e.target.value})}
                placeholder='Header: Value&#10;Another: Value'
                className="glass-input w-full font-mono resize-none py-2 px-3 text-theme-primary text-[10px]"
              />
            </div>
            <div className="space-y-1 text-left">
              <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">Server Headers:</label>
              <textarea 
                rows="6"
                value={config.server_headers}
                onChange={(e) => setConfig({...config, server_headers: e.target.value})}
                placeholder='Server: Apache&#10;X-Powered-By: PHP'
                className="glass-input w-full font-mono resize-none py-2 px-3 text-theme-primary text-[10px]"
              />
            </div>
          </div>
        </div>
      )}

      {activeTab === 'error' && (
        <div className="space-y-2 animate-in fade-in slide-in-from-top-1 duration-200">
          <div className="space-y-1 text-left">
            <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">Page Error (HTML)</label>
            <textarea 
              rows="12"
              value={config.page_error}
              onChange={(e) => setConfig({...config, page_error: e.target.value})}
              className="glass-input w-full font-mono resize-none py-2 px-3 text-theme-primary text-[10px]"
            />
          </div>
        </div>
      )}

      {activeTab === 'payload' && (
        <div className="space-y-2 animate-in fade-in slide-in-from-top-1 duration-200">
          <div className="space-y-1 text-left">
            <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">Page Payload (must include &lt;&lt;&lt;PAYLOAD_DATA&gt;&gt;&gt;)</label>
            <textarea 
              rows="12"
              value={config.page_payload}
              onChange={(e) => setConfig({...config, page_payload: e.target.value})}
              className="glass-input w-full font-mono resize-none py-2 px-3 text-theme-primary text-[10px]"
            />
          </div>
        </div>
      )}

      {activeTab === 'extra' && (
        <div className="space-y-2 animate-in fade-in slide-in-from-top-1 duration-200">
          <div className="space-y-1 text-left">
            <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">Proxy</label>
            <input 
              type="text"
              value={config.proxy}
              onChange={(e) => setConfig({...config, proxy: e.target.value})}
              placeholder="http://user:pass@host:port"
              className="glass-input w-full font-mono py-1.5 px-3 text-theme-primary text-xs"
            />
          </div>
        </div>
      )}
    </>
  );

  const renderDoHFields = () => (
    <div className="grid grid-cols-2 gap-2 animate-in fade-in slide-in-from-top-1 duration-200">
      <div className="space-y-1 text-left">
        <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">Host & port (Bind):</label>
        <div className="flex space-x-2">
          <input 
            type="text"
            value={config.bind_host}
            onChange={(e) => setConfig({...config, bind_host: e.target.value})}
            className="glass-input flex-1 font-mono py-1.5 px-3 text-theme-primary text-xs"
          />
          <input 
            type="text"
            value={config.bind_port}
            onChange={(e) => setConfig({...config, bind_port: e.target.value})}
            className="glass-input w-20 font-mono py-1.5 px-3 text-theme-primary text-xs text-center"
          />
        </div>
      </div>
      <div className="space-y-1 text-left">
        <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">Domain</label>
        <input 
          type="text"
          value={config.domain}
          onChange={(e) => setConfig({...config, domain: e.target.value})}
          placeholder="ns1.example.com"
          className="glass-input w-full font-mono py-1.5 px-3 text-theme-primary text-xs"
        />
      </div>
      <div className="col-span-2 space-y-1 text-left">
        <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">DoH Resolvers (Comma separated)</label>
        <input 
          type="text"
          value={config.doh_urls}
          onChange={(e) => setConfig({...config, doh_urls: e.target.value})}
          placeholder="https://8.8.8.8/dns-query"
          className="glass-input w-full font-mono py-1.5 px-3 text-theme-primary text-xs"
        />
      </div>
      <div className="col-span-2 space-y-1 text-left">
        <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">User Agent</label>
        <input 
          type="text"
          value={config.user_agent}
          onChange={(e) => setConfig({...config, user_agent: e.target.value})}
          className="glass-input w-full font-mono py-1.5 px-3 text-theme-primary text-xs"
        />
      </div>
    </div>
  );

  const renderSMBFields = () => (
    <div className="space-y-2 animate-in fade-in slide-in-from-top-1 duration-200">
      <div className="space-y-1 text-left">
        <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">Pipe Name</label>
        <div className="flex items-center space-x-2">
          <span className="text-theme-muted font-mono text-xs">\\.\pipe\</span>
          <input 
            type="text"
            value={config.pipe_name}
            onChange={(e) => setConfig({...config, pipe_name: e.target.value})}
            className="glass-input flex-1 font-mono py-1.5 px-3 text-theme-primary text-xs"
          />
        </div>
      </div>
    </div>
  );

  const renderTCPFields = () => (
    <div className="space-y-2 animate-in fade-in slide-in-from-top-1 duration-200">
      <div className="space-y-1 text-left">
        <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">Bind Port</label>
        <input 
          type="number"
          value={config.bind_port_tcp}
          onChange={(e) => setConfig({...config, bind_port_tcp: e.target.value})}
          className="glass-input w-full font-mono py-1.5 px-3 text-theme-primary text-xs"
        />
      </div>
    </div>
  );

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title={editMode ? "Edit Listener" : "Create Listener"}
      width="max-w-3xl"
    >
      <div className="flex flex-col h-full bg-theme-glass-panel">
        {/* Top Header Section */}
        <div className="p-3 grid grid-cols-12 gap-2 bg-theme-glass-panel border-b border-theme-glass-light">
          {/* Row 1: Basic Info */}
          <div className="col-span-4 space-y-1 text-left">
            <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">Name</label>
            <input 
              type="text"
              value={listenerName}
              onChange={(e) => {
                setListenerName(e.target.value);
                if (saveAsProfile) setProfileName(e.target.value);
              }}
              placeholder="e.g. HTTP_External"
              disabled={editMode}
              className="glass-input w-full font-mono py-1.5 px-3 text-theme-primary text-xs"
            />
          </div>
          
          <div className="col-span-4 space-y-1 text-left">
            <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">Type</label>
            <div className="relative">
              <select
                value={selectedType}
                onChange={(e) => setSelectedType(e.target.value)}
                disabled={editMode}
                className="glass-input w-full font-mono py-1.5 px-3 pr-8 text-theme-primary text-xs appearance-none"
              >
                {displayListeners.map((type) => (
                  <option key={type.id} value={type.id} className="bg-theme-glass-panel text-theme-primary">
                    {type.label}
                  </option>
                ))}
              </select>
              <ChevronDown size={12} className="absolute right-2 top-1/2 -translate-y-1/2 text-theme-muted pointer-events-none" />
            </div>
          </div>

          <div className="col-span-4 space-y-1 relative text-left">
            <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1">Profile</label>
            <div className="relative group">
              <input 
                type="text"
                value={profileName}
                onChange={(e) => setProfileName(e.target.value)}
                placeholder="Ident..."
                disabled={!saveAsProfile || editMode}
                className="glass-input w-full font-mono italic pr-8 py-1.5 px-3 text-theme-primary text-[10px]"
              />
              <button 
                onClick={() => setSaveAsProfile(!saveAsProfile)}
                className={cn(
                  "absolute right-1.5 top-1/2 -translate-y-1/2 p-1 rounded-md transition-colors",
                  saveAsProfile ? "text-theme-accent bg-theme-glass" : "text-theme-muted"
                )}
              >
                <Check size={12} />
              </button>
            </div>
          </div>

          {/* Row 2: Encrypt Key (Common) */}
          <div className="col-span-12 space-y-1 text-left">
            <label className="text-[9px] font-black uppercase text-theme-muted tracking-widest ml-1 flex items-center">
              <Key size={10} className="mr-1" />
              Encryption Key
            </label>
            <div className="flex space-x-2">
              <input 
                type="text"
                value={config.encrypt_key}
                onChange={(e) => setConfig({...config, encrypt_key: e.target.value})}
                className="glass-input flex-1 font-mono py-1.5 px-3 text-theme-primary text-xs"
              />
              <button 
                onClick={generateKey}
                className="glass-btn px-3 flex items-center justify-center hover:text-theme-accent transition-colors"
                title="Generate Random Key"
              >
                <RefreshCw size={12} />
              </button>
            </div>
          </div>
        </div>

        {/* Tab Navigation - Only for HTTP */}
        {selectedType === 'BeaconHTTP' && (
          <div className="px-3 pt-2 bg-theme-glass-panel border-b border-theme-glass-light flex space-x-4">
            {[
              { id: 'main', label: 'Main settings' },
              { id: 'headers', label: 'HTTP Headers' },
              { id: 'error', label: 'Page Error' },
              { id: 'payload', label: 'Page Payload' },
              { id: 'extra', label: 'Advanced' }
            ].map(tab => (
              <button
                key={tab.id}
                onClick={() => setActiveTab(tab.id)}
                className={cn(
                  "pb-2 px-1 text-[10px] font-black uppercase tracking-widest transition-all border-b-2",
                  activeTab === tab.id 
                    ? "border-theme-accent text-theme-accent" 
                    : "border-transparent text-theme-muted hover:text-theme-primary hover:border-theme-glass-light"
                )}
              >
                {tab.label}
              </button>
            ))}
          </div>
        )}

        {/* Form Content */}
        <div className="p-3 bg-theme-glass-panel flex-1 overflow-y-auto custom-scrollbar">
          {isDynamic && currentListener.ui_schema ? (
            <DynamicFormRenderer 
              schema={currentListener.ui_schema}
              value={config}
              onChange={(newConfig) => setConfig(prev => ({ ...prev, ...newConfig }))}
            />
          ) : (
            <>
              {selectedType === 'BeaconHTTP' && renderHTTPFields()}
              {selectedType === 'BeaconDoH' && renderDoHFields()}
              {selectedType === 'BeaconSMB' && renderSMBFields()}
              {selectedType === 'BeaconTCP' && renderTCPFields()}
            </>
          )}
        </div>

        {/* Footer Actions */}
        <div className="p-3 border-t border-theme-glass-light flex items-center justify-between bg-theme-glass-panel shrink-0">
          <div className="flex items-center space-x-2 text-theme-muted italic text-[9px] uppercase font-bold tracking-tighter">
            <Info size={12} className="text-theme-accent" />
            <span>Infrastructure deployment required</span>
          </div>
          <div className="flex items-center space-x-3">
            <button 
              onClick={onClose}
              className="glass-btn px-4 py-1.5 text-[9px] font-bold text-theme-muted hover:text-theme-primary transition-all uppercase tracking-widest"
            >
              Cancel
            </button>
            <button 
              onClick={handleCreate}
              className="glass-btn-primary px-6 py-1.5 text-[9px] font-black uppercase tracking-widest shadow-glow-sm hover:shadow-glow flex items-center space-x-2 text-white"
            >
              <Save size={12} className="text-white" />
              <span>{editMode ? 'Commit Update' : 'Initialize Listener'}</span>
            </button>
          </div>
        </div>
      </div>
    </Modal>
  );
};

export default CreateListenerDialog;
