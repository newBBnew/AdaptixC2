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
        apiEndpoint: '/api/proxy',
        wsEndpoint: '/api/proxy/connect',
        teamserverUrl: 'https://localhost:4321/endpoint',
        endpointPrefix: '' 
      };
    }
    return baseConfig;
  });

  useEffect(() => {
    localStorage.setItem('adaptix_config', JSON.stringify(config));
  }, [config]);

  const updateConfigFromUrl = (inputUrl) => {
    if (!inputUrl || inputUrl.trim() === '' || inputUrl.trim() === 'https://' || inputUrl.trim() === 'http://') {
      return null;
    }
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
        // We use /api/proxy to match the gateway's Any("/proxy/*path") route.
        apiEndpoint: isLocal ? `/api/proxy` : `${origin}${path}`,
        // Same for WebSocket
        wsEndpoint: isLocal ? `/api/proxy/connect` : `${origin.replace('http', 'ws')}${path}/connect`
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
