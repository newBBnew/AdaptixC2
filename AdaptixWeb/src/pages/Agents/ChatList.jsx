import React, { useState, useEffect, useRef } from 'react';
import { 
  MessageSquare, 
  Search, 
  ChevronLeft, 
  ChevronRight, 
  X, 
  Send,
  Trash2
} from 'lucide-react';
import { cn } from '../../utils/cn';

const ChatList = () => {
  const [messages, setMessages] = useState([
    { time: Date.now() / 1000 - 600, username: 'admin', message: 'Hello team, welcome to Adaptix C2.' },
    { time: Date.now() / 1000 - 540, username: 'operator1', message: 'Ready for action. Beacons are looking good.' },
  ]);
  const [inputValue, setInputValue] = useState('');
  const [searchQuery, setSearchQuery] = useState('');
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const scrollRef = useRef(null);
  const currentUser = 'admin'; // Should come from context later

  useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [messages]);

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

  const handleSendMessage = (e) => {
    e.preventDefault();
    if (!inputValue.trim()) return;

    // Simulate sending
    const newMessage = {
      time: Date.now() / 1000,
      username: currentUser,
      message: inputValue.trim(),
    };
    setMessages([...messages, newMessage]);
    setInputValue('');
    
    // In reality, this would call HttpReqChatSendMessageAsync
  };

  return (
    <div className="flex flex-col h-full bg-dark-950 text-[12px] font-mono select-text overflow-hidden">
      {/* 1. Header */}
      <div className="flex items-center justify-between px-3 py-1.5 bg-dark-800 border-b border-dark-700 shrink-0 select-none">
        <div className="flex items-center space-x-3">
          <div className="flex items-center space-x-2 px-2 py-0.5 rounded bg-accent-secondary/10 border border-accent-secondary/20">
            <MessageSquare className="w-3.5 h-3.5 text-accent-secondary" />
            <span className="text-[10px] font-black uppercase tracking-widest text-accent-secondary">Team Chat</span>
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

        <div className="flex items-center space-x-2">
          <button 
            onClick={() => setMessages([])}
            className="p-1.5 rounded hover:bg-dark-700 text-gray-400 hover:text-accent-danger transition-all"
            title="Clear Chat (Ctrl+L)"
          >
            <Trash2 size={14} />
          </button>
        </div>
      </div>

      {/* 2. Search Bar */}
      {isSearchVisible && (
        <div className="flex items-center px-4 py-2 bg-dark-800/50 border-b border-dark-700 animate-in slide-in-from-top-2 duration-200 shrink-0 select-none">
          <div className="flex items-center space-x-2 mr-4 text-gray-500">
            <button className="p-0.5 hover:text-white transition-colors"><ChevronLeft size={14}/></button>
            <button className="p-0.5 hover:text-white transition-colors"><ChevronRight size={14}/></button>
            <span className="text-[10px] font-bold min-w-[40px] text-center">0 of 0</span>
          </div>
          <div className="relative flex-1 max-w-md">
            <input 
              type="text" 
              autoFocus
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Find in chat..." 
              className="w-full bg-dark-950/50 border border-dark-600 rounded px-3 py-1 text-[11px] text-gray-300 outline-none focus:border-accent-primary/50 placeholder:text-gray-700"
            />
            {searchQuery && (
              <button onClick={() => setSearchQuery('')} className="absolute right-2 top-1/2 -translate-y-1/2 text-gray-600 hover:text-gray-400">
                <X size={12} />
              </button>
            )}
          </div>
        </div>
      )}

      {/* 3. Chat Content */}
      <div 
        ref={scrollRef}
        className="flex-1 overflow-auto p-4 custom-scrollbar bg-[#0a0a0a]"
      >
        <div className="space-y-1">
          {messages.map((msg, i) => (
            <div key={i} className="flex items-start space-x-2 group">
              <span className="text-gray-600 shrink-0">
                {new Date(msg.time * 1000).toLocaleTimeString([], { hour12: false })}
              </span>
              <span className="text-gray-400 font-bold shrink-0">
                [<span className={cn(msg.username === currentUser ? "text-accent-secondary" : "text-green-500")}>
                  {msg.username}
                </span>] ::
              </span>
              <span className="text-white break-all whitespace-pre-wrap">
                {msg.message}
              </span>
            </div>
          ))}
          {messages.length === 0 && (
            <div className="py-20 flex flex-col items-center justify-center opacity-10 select-none">
              <MessageSquare size={64} />
              <p className="mt-4 text-sm font-bold tracking-widest uppercase">No messages</p>
            </div>
          )}
        </div>
      </div>

      {/* 4. Input Area */}
      <form 
        onSubmit={handleSendMessage}
        className="px-3 py-2 bg-dark-800 border-t border-dark-700 flex items-center space-x-3 shrink-0"
      >
        <div className="text-[11px] font-black uppercase text-accent-secondary px-2 py-1 rounded bg-accent-secondary/10 border border-accent-secondary/20">
          {currentUser}
        </div>
        <div className="flex-1 relative">
          <input 
            type="text"
            value={inputValue}
            onChange={(e) => setInputValue(e.target.value)}
            placeholder="Type a message..."
            className="w-full bg-dark-950 border border-dark-600 rounded-md py-1.5 px-3 text-[12px] text-white outline-none focus:border-accent-primary/50 transition-all"
          />
        </div>
        <button 
          type="submit"
          disabled={!inputValue.trim()}
          className="p-1.5 rounded-md bg-accent-primary/20 text-accent-primary hover:bg-accent-primary/30 disabled:opacity-50 disabled:cursor-not-allowed transition-all"
        >
          <Send size={16} />
        </button>
      </form>
    </div>
  );
};

export default ChatList;
