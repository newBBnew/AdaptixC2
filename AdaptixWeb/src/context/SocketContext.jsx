import React, { createContext, useContext, useEffect, useRef, useState } from 'react';
import { useConfig } from './ConfigContext';
import api from '../api/agent';
import { PacketType } from '../constants/packetTypes';

const SocketContext = createContext();

export const useSocket = () => useContext(SocketContext);

export const SocketProvider = ({ children }) => {
  const { config } = useConfig();
  const [isConnected, setIsConnected] = useState(false);
  const [isSyncing, setIsSyncing] = useState(false);
  const [syncProgress, setSyncProgress] = useState({ current: 0, total: 0 });
  const [lastMessage, setLastMessage] = useState(null);
  const socketRef = useRef(null);
  const listenersRef = useRef([]);

  const connect = () => {
    if (socketRef.current) return;

    const token = localStorage.getItem('adaptix_token');
    if (!token) return;

    // Build WebSocket URL
    let wsUrl = config.wsEndpoint;
    
    // If wsEndpoint is a relative path, build full URL
    if (wsUrl.startsWith('/')) {
      const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
      wsUrl = `${wsProtocol}//${window.location.host}${wsUrl}`;
    }

    console.log('[WebSocket] Connecting to:', wsUrl);
    const socket = new WebSocket(wsUrl);

    socket.onopen = async () => {
      console.log('[WebSocket] Connected');
      setIsConnected(true);
      
      // Trigger initial synchronization via POST /sync
      // This matches the Qt client's behavior after connecting
      try {
        await api.post(`/sync`, {});
        console.log('[WebSocket] Sync request dispatched');
      } catch (err) {
        console.error('[WebSocket] Sync request failed:', err);
      }
    };

    socket.onmessage = (event) => {
      try {
        let data;
        if (typeof event.data === 'string') {
          data = JSON.parse(event.data);
        } else {
          // Handle binary if necessary, though server usually sends JSON for control
          console.warn('[WebSocket] Received non-string data');
          return;
        }

        handlePacket(data);
      } catch (err) {
        console.error('[WebSocket] Error parsing message:', err);
      }
    };

    socket.onclose = () => {
      console.log('[WebSocket] Disconnected');
      setIsConnected(false);
      setIsSyncing(false);
      socketRef.current = null;
      // Auto reconnect after 5s
      setTimeout(connect, 5000);
    };

    socket.onerror = (err) => {
      console.error('[WebSocket] Error:', err);
      socket.close();
    };

    socketRef.current = socket;
  };

  const handlePacket = (packet) => {
    const type = packet.type;

    // 1. Handle Sync Lifecycle (Matching Qt Client hex types)
    if (type === PacketType.SYNC_START) {
      setIsSyncing(true);
      const count = packet.count || 0;
      setSyncProgress({ current: 0, total: count });
      console.log(`[WebSocket] Sync started. Total packets: ${count}`);
      return;
    }

    if (type === PacketType.SYNC_FINISH) {
      setIsSyncing(false);
      setSyncProgress(prev => ({ ...prev, current: prev.total }));
      console.log('[WebSocket] Sync finished');
      return;
    }

    // 2. Handle Batched Packets
    if (type === PacketType.SYNC_BATCH || type === PacketType.SYNC_CATEGORY_BATCH) {
      if (packet.packets && Array.isArray(packet.packets)) {
        packet.packets.forEach(p => handlePacket(p));
      }
      return;
    }

    // 3. Update Sync Progress for individual packets during sync phase
    if (isSyncing) {
      setSyncProgress(prev => ({ 
        ...prev, 
        current: Math.min(prev.current + 1, prev.total) 
      }));
    }

    // 4. Dispatch to all registered listeners (AgentContext, etc.)
    setLastMessage(packet);
    listenersRef.current.forEach(callback => callback(packet));
  };

  const addListener = (callback) => {
    listenersRef.current.push(callback);
    return () => {
      listenersRef.current = listenersRef.current.filter(l => l !== callback);
    };
  };

  const sendMessage = (data) => {
    if (socketRef.current && isConnected) {
      socketRef.current.send(JSON.stringify(data));
    }
  };

  useEffect(() => {
    const token = localStorage.getItem('adaptix_token');
    if (token) connect();
    
    return () => {
      if (socketRef.current) {
        socketRef.current.close();
        socketRef.current = null;
      }
    };
  }, [config.wsEndpoint]); // Reconnect if endpoint changes

  return (
    <SocketContext.Provider value={{ 
      isConnected, 
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
