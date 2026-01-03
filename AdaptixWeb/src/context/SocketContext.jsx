import React, { createContext, useContext, useEffect, useRef, useState, useCallback } from 'react';
import { useConfig } from './ConfigContext';
import api from '../api/agent';
import { PacketType } from '../constants/packetTypes';

const SocketContext = createContext();

export const useSocket = () => useContext(SocketContext);

export const SocketProvider = ({ children }) => {
  const { config } = useConfig();
  const [connectionStatus, setConnectionStatus] = useState('disconnected'); // 'disconnected' | 'connecting' | 'connected'
  const [isSyncing, setIsSyncing] = useState(false);
  const [syncProgress, setSyncProgress] = useState({ current: 0, total: 0 });
  const [lastMessage, setLastMessage] = useState(null);
  const socketRef = useRef(null);
  const listenersRef = useRef([]);
  const messageQueueRef = useRef([]);
  const reconnectTimeoutRef = useRef(null);

  const connect = useCallback(() => {
    if (socketRef.current || connectionStatus === 'connecting') return;

    const token = localStorage.getItem('adaptix_token');
    if (!token) {
      console.warn('[WebSocket] Missing auth token, skipping connection');
      return;
    }

    setConnectionStatus('connecting');

    // Build WebSocket URL
    let wsUrl = config.wsEndpoint;
    if (wsUrl.startsWith('/')) {
      const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
      wsUrl = `${wsProtocol}//${window.location.host}${wsUrl}`;
    } else if (wsUrl.startsWith('http')) {
      wsUrl = wsUrl.replace(/^http/, 'ws');
    }
    
    const separator = wsUrl.includes('?') ? '&' : '?';
    wsUrl = `${wsUrl}${separator}token=${encodeURIComponent(token)}&version=v1.0`;

    try {
      const socket = new WebSocket(wsUrl);
      socket.binaryType = 'arraybuffer';

      socket.onopen = () => {
        console.log('[WebSocket] Connection established');
        setConnectionStatus('connected');
        
        // Clear any pending reconnect
        if (reconnectTimeoutRef.current) {
          clearTimeout(reconnectTimeoutRef.current);
          reconnectTimeoutRef.current = null;
        }

        // Process queued messages
        while (messageQueueRef.current.length > 0) {
          const msg = messageQueueRef.current.shift();
          socket.send(JSON.stringify(msg));
        }

        // Start Sync
        setTimeout(async () => {
          try {
            await api.post(`/sync`, {});
          } catch (err) {
            console.error('[WebSocket] Initial sync trigger failed:', err);
          }
        }, 300);
      };

      socket.onmessage = (event) => {
        try {
          let data;
          if (typeof event.data === 'string') {
            data = JSON.parse(event.data);
          } else if (event.data instanceof ArrayBuffer) {
            const decoder = new TextDecoder('utf-8');
            data = JSON.parse(decoder.decode(new Uint8Array(event.data)).trim());
          }

          if (data) {
            handlePacket(data);
          }
        } catch (err) {
          console.error('[WebSocket] Message decode error:', err);
        }
      };

      socket.onclose = (event) => {
        setConnectionStatus('disconnected');
        setIsSyncing(false);
        socketRef.current = null;
        
        console.log(`[WebSocket] Connection closed (code: ${event.code})`);
        
        // Auto-reconnect logic
        if (event.code !== 1000 && !reconnectTimeoutRef.current) {
          reconnectTimeoutRef.current = setTimeout(() => {
            reconnectTimeoutRef.current = null;
            connect();
          }, 5000);
        }
      };

      socket.onerror = (err) => {
        console.error('[WebSocket] Socket encountered error:', err);
        socket.close();
      };

      socketRef.current = socket;
    } catch (e) {
      setConnectionStatus('disconnected');
      console.error('[WebSocket] Failed to initiate connection:', e);
    }
  }, [config.wsEndpoint, connectionStatus]);

  const handlePacket = useCallback((packet) => {
    const type = packet.type;

    if (type === PacketType.SYNC_START) {
      setIsSyncing(true);
      setSyncProgress({ current: 0, total: packet.count || 0 });
      return;
    }

    if (type === PacketType.SYNC_FINISH) {
      setIsSyncing(false);
      setSyncProgress(prev => ({ ...prev, current: prev.total }));
      return;
    }

    if (type === PacketType.SYNC_BATCH || type === PacketType.SYNC_CATEGORY_BATCH) {
      if (packet.packets) {
        packet.packets.forEach(p => handlePacket(p));
      }
      return;
    }

    if (isSyncing) {
      setSyncProgress(prev => ({ 
        ...prev, 
        current: Math.min(prev.current + 1, prev.total) 
      }));
    }

    setLastMessage(packet);
    listenersRef.current.forEach(callback => callback(packet));
  }, [isSyncing]);

  const addListener = useCallback((callback) => {
    listenersRef.current.push(callback);
    return () => {
      listenersRef.current = listenersRef.current.filter(l => l !== callback);
    };
  }, []);

  const sendMessage = useCallback((data) => {
    if (socketRef.current && socketRef.current.readyState === WebSocket.OPEN) {
      socketRef.current.send(JSON.stringify(data));
    } else {
      console.log('[WebSocket] Socket not ready, queueing message');
      messageQueueRef.current.push(data);
    }
  }, []);

  useEffect(() => {
    connect();
    return () => {
      if (socketRef.current) {
        socketRef.current.close(1000, 'Normal closure');
      }
      if (reconnectTimeoutRef.current) {
        clearTimeout(reconnectTimeoutRef.current);
      }
    };
  }, []);

  return (
    <SocketContext.Provider value={{ 
      isConnected: connectionStatus === 'connected',
      connectionStatus,
      isSyncing, 
      syncProgress, 
      lastMessage, 
      addListener, 
      sendMessage 
    }}>
      {children}
    </SocketContext.Provider>
  );
};
