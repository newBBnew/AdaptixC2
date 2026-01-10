import React, { useState, useEffect, useRef } from 'react';
import { 
  MessageSquare, 
  Search, 
  ChevronLeft, 
  ChevronRight, 
  X, 
  Send,
  Trash2,
  User,
  Shield,
  Activity
} from 'lucide-react';
import { cn } from '../../utils/cn';
import { useSocket } from '../../context/SocketContext';
import { useAgents } from '../../context/AgentContext';
import api from '../../api/agent';

const ChatList = () => {
  const { chatMessages, globalSearchQuery } = useAgents();
  const [messages, setMessages] = useState([]);
  const [inputValue, setInputValue] = useState('');
  const [searchQuery, setSearchQuery] = useState('');
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const scrollRef = useRef(null);
  const currentUser = localStorage.getItem('adaptix_user') || 'admin';

  const displayedMessages = messages.length > 0 ? messages : chatMessages;

  useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [displayedMessages]);

  useEffect(() => {
    const handleKeyDown = (e) => {
      if ((e.ctrlKey || e.metaKey) && e.key === 'f') {
        e.preventDefault();
        setIsSearchVisible(prev => !prev);
      }
      if ((e.ctrlKey || e.metaKey) && e.key === 'l') {
        e.preventDefault();
        setMessages([]);
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, []);

  const handleSendMessage = async (e) => {
    e.preventDefault();
    if (!inputValue.trim()) return;

    const text = inputValue.trim();
    setInputValue('');

    try {
      await api.post('/chat/send', { message: text });
    } catch (err) {
      console.error('Failed to send message:', err);
    }
  };

  const filteredMessages = displayedMessages.filter(msg => {
    const query = (searchQuery || globalSearchQuery).toLowerCase();
    return msg.message?.toLowerCase().includes(query) ||
           msg.username?.toLowerCase().includes(query);
  });

  return (
    <div className="flex flex-col h-full w-full select-none overflow-hidden">
      {/* 1. Header */}
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
        </div>

        <div className="flex items-center space-x-2 pr-1">
          <button 
            onClick={() => setMessages([])}
            className="p-2 glass-btn text-theme-muted hover:text-theme-danger transition-all"
            title="Clear Chat (Ctrl+L)"
          >
            <Trash2 size={16} />
          </button>
        </div>
      </div>

      {/* 2. Search Bar */}
      {isSearchVisible && (
        <div className="flex items-center px-4 py-2 glass-card-sm border-b border-theme-glass-light shrink-0 animate-in slide-in-from-top-1 duration-200">
          <div className="relative flex-1 max-w-md">
            <Search className="w-4 h-4 absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted" />
            <input 
              type="text" 
              autoFocus
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search messages..." 
              className="glass-input w-full pl-10 py-2 text-sm text-theme-primary placeholder:text-theme-muted"
            />
          </div>
        </div>
      )}

      {/* 3. Chat Content */}
      <div 
        ref={scrollRef}
        className="flex-1 overflow-auto p-4 custom-scrollbar glass-panel font-mono text-[12px] leading-relaxed text-theme-primary"
      >
        <div className="space-y-2">
          {filteredMessages.map((msg, i) => (
            <div key={i} className="flex items-start space-x-3 group py-1.5 hover:bg-theme-hover transition-all px-2 -mx-2 rounded-lg">
              <span className="text-theme-muted shrink-0 select-none font-medium text-[11px] mt-0.5 font-bold">
                [{new Date(msg.time * 1000).toLocaleTimeString([], { hour12: false, hour: '2-digit', minute: '2-digit' })}]
              </span>
              <div className="flex-1 min-w-0">
                <span className={cn(
                  "font-bold mr-2",
                  msg.username === currentUser ? "text-theme-accent-secondary" : "text-theme-accent"
                )}>
                  @{msg.username || 'Unknown'}
                </span>
                <span className="text-theme-muted mr-2 opacity-50 select-none">:</span>
                <span className="text-theme-primary break-all whitespace-pre-wrap">
                  {msg.message}
                </span>
              </div>
            </div>
          ))}
          {filteredMessages.length === 0 && (
            <div className="h-full flex flex-col items-center justify-center opacity-40 select-none py-24 space-y-4">
              <MessageSquare size={64} className="text-theme-muted" strokeWidth={1} />
              <div className="text-center">
                <p className="text-sm font-medium tracking-wider text-theme-primary uppercase">No Messages</p>
                <p className="text-xs text-theme-muted mt-1 font-bold uppercase">Start a conversation with your team</p>
              </div>
            </div>
          )}
        </div>
      </div>

      {/* 4. Input Area */}
      <div className="flex flex-col glass-card-sm border-t border-theme-glass-light">
        <form 
          onSubmit={handleSendMessage}
          className="flex items-center px-3 py-2 space-x-3"
        >
          <div className="flex items-center space-x-2 ml-2 shrink-0 glass-btn px-3 py-1.5 rounded-lg border-theme-glass-light">
            <User size={14} className="text-theme-accent-secondary" />
            <span className="text-xs font-semibold text-theme-accent-secondary">{currentUser}</span>
          </div>
          <div className="flex-1 relative">
            <input 
              type="text"
              value={inputValue}
              onChange={(e) => setInputValue(e.target.value)}
              placeholder="Type a message..."
              className="glass-input w-full py-2 pl-4 pr-12 text-sm text-theme-primary placeholder:text-theme-muted"
            />
            <button 
              type="submit"
              className="absolute right-2 top-1/2 -translate-y-1/2 p-1.5 glass-btn text-theme-muted hover:text-theme-accent transition-all rounded-lg"
            >
              <Send size={16} />
            </button>
          </div>
        </form>
        <div className="px-4 py-1 flex items-center justify-between text-[9px] font-black text-theme-muted uppercase tracking-widest bg-theme-glass-panel">
          <div className="flex items-center space-x-2">
            <div className="w-1.5 h-1.5 rounded-full bg-theme-success shadow-glow-sm animate-pulse" />
            <span>Encrypted Relay Channel</span>
          </div>
          <span className="opacity-50">v1.0.4-CHAT</span>
        </div>
      </div>
    </div>
  );
};

export default ChatList;
