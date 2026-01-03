import React, { createContext, useContext, useState, useEffect } from 'react';

const ConfigContext = createContext();

export const useConfig = () => useContext(ConfigContext);

export const ConfigProvider = ({ children }) => {
  const [config, setConfig] = useState(() => {
    const saved = localStorage.getItem('adaptix_config');
    let baseConfig;
    if (saved) {
      baseConfig = JSON.parse(saved);
    } else {
      baseConfig = {
        baseUrl: window.location.origin,
        apiEndpoint: '/api',
        wsEndpoint: '/api/connect',
        teamserverUrl: localStorage.getItem('adaptix_url') || 'https://localhost:4321/endpoint',
        endpointPrefix: '' // Removed '/endpoint' to match backend routes
      };
    }

    // Migration/Normalization: Always ensure local connections use the proxy
    const isLocal = baseConfig.teamserverUrl && (baseConfig.teamserverUrl.includes('localhost') || baseConfig.teamserverUrl.includes('127.0.0.1'));
    if (isLocal) {
      // Re-calculate based on saved teamserverUrl
      try {
        let urlStr = baseConfig.teamserverUrl.trim();
        if (!urlStr.startsWith('http://') && !urlStr.startsWith('https://')) {
          urlStr = 'https://' + urlStr;
        }
        const urlObj = new URL(urlStr);
        let path = urlObj.pathname;
        if (path.length > 1 && path.endsWith('/')) path = path.slice(0, -1);
        const prefix = path === '/' ? '' : path;
        
        baseConfig.endpointPrefix = prefix;
        baseConfig.apiEndpoint = `/api${prefix}`;
        baseConfig.wsEndpoint = `/api${prefix}/connect`;
      } catch (e) {
        // Fallback
        baseConfig.apiEndpoint = '/api';
        baseConfig.wsEndpoint = '/api/connect';
      }
    }
    return baseConfig;
  });

  useEffect(() => {
    localStorage.setItem('adaptix_config', JSON.stringify(config));
  }, [config]);

  const updateConfigFromUrl = (inputUrl) => {
    try {
      // Normalize URL (ensure protocol exists for URL constructor)
      let normalizedUrl = inputUrl.trim();
      if (!normalizedUrl.startsWith('http://') && !normalizedUrl.startsWith('https://')) {
        normalizedUrl = 'https://' + normalizedUrl;
      }

      const urlObj = new URL(normalizedUrl);
      const origin = urlObj.origin; // e.g., https://1.2.3.4:4321
      let path = urlObj.pathname;   // e.g., /endpoint
      
      // If path is just '/', we do NOT default to /endpoint anymore.
      // The user must specify the full path if the backend uses a prefix.
      // if (path === '/') path = '/endpoint';
      
      const isLocal = normalizedUrl.includes('localhost') || normalizedUrl.includes('127.0.0.1');
      
      // Ensure path doesn't end with slash unless it's just root
      if (path.length > 1 && path.endsWith('/')) {
        path = path.slice(0, -1);
      }
      
      const newConfig = {
        ...config,
        baseUrl: origin,
        teamserverUrl: normalizedUrl,
        endpointPrefix: path === '/' ? '' : path,
        // For local, we prefix with /api so the proxy catches it.
        // We ALSO append the user-defined path (endpointPrefix) so the proxy can just strip '/api'
        // and forward the full path (e.g. /endpoint/...) to the backend.
        apiEndpoint: isLocal ? `/api${path === '/' ? '' : path}` : `${origin}${path}`,
        // Same for WebSocket
        wsEndpoint: isLocal ? `/api${path === '/' ? '' : path}/connect` : `${origin.replace('http', 'ws')}${path}/connect`
      };
      
      setConfig(newConfig);
      return newConfig;
    } catch (e) {
      console.error('Invalid URL for config:', inputUrl);
      return null;
    }
  };

  return (
    <ConfigContext.Provider value={{ config, setConfig, updateConfigFromUrl }}>
      {children}
    </ConfigContext.Provider>
  );
};
