import React, { useEffect, useRef, useState } from 'react';
import { Terminal } from 'xterm';
import { FitAddon } from 'xterm-addon-fit';
import { WebLinksAddon } from 'xterm-addon-web-links';
import 'xterm/css/xterm.css';
import { 
  Play, 
  Square, 
  RefreshCw, 
  Terminal as TerminalIcon,
  Settings,
  ChevronDown
} from 'lucide-react';
import { cn } from '../../utils/cn';
import { useTheme } from '../../context/ThemeContext';

const RemoteTerminal = ({ agent }) => {
  const { theme } = useTheme();
  const terminalRef = useRef(null);
  const xtermRef = useRef(null);
  const fitAddonRef = useRef(null);
  const wsRef = useRef(null);
  
  const [status, setStatus] = useState('stopped'); // stopped, connecting, running
  const [program, setProgram] = useState('');
  const [customProgram, setCustomProgram] = useState('');
  const [showProgramMenu, setShowProgramMenu] = useState(false);

  // Get default programs based on OS
  const getPrograms = () => {
    if (agent.a_os === 1) { // Windows
      return [
        { label: 'Cmd', path: 'C:\\Windows\\System32\\cmd.exe' },
        { label: 'PowerShell', path: 'C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe' },
        { label: 'Custom...', path: '' }
      ];
    } else if (agent.a_os === 2) { // Linux
      return [
        { label: 'Shell', path: '/bin/sh' },
        { label: 'Bash', path: '/bin/bash' },
        { label: 'Custom...', path: '' }
      ];
    } else { // macOS
      return [
        { label: 'ZSH', path: '/bin/zsh' },
        { label: 'Bash', path: '/bin/bash' },
        { label: 'Shell', path: '/bin/sh' },
        { label: 'Custom...', path: '' }
      ];
    }
  };

  const programs = getPrograms();

  useEffect(() => {
    // Set default program
    if (programs.length > 0 && !program) {
      setProgram(programs[0].path);
    }
  }, [agent.a_os]);

  // Initialize xterm
  useEffect(() => {
    if (!terminalRef.current || xtermRef.current) return;

    const term = new Terminal({
      cursorBlink: true,
      fontSize: 12,
      fontFamily: '"JetBrains Mono", "Fira Code", monospace',
      theme: {
        background: '#050505',
        foreground: theme.colors.primary,
        cursor: theme.colors.primary,
        cursorAccent: '#000000',
        selectionBackground: `${theme.colors.primary}4D`,
        black: '#000000',
        red: theme.colors.danger,
        green: theme.colors.success,
        yellow: '#FDFD96',
        blue: theme.colors.secondary,
        magenta: '#A01641',
        cyan: '#00cccc',
        white: '#BEBEBE',
        brightBlack: '#666666',
        brightRed: '#ff6b6b',
        brightGreen: '#33ff33',
        brightYellow: '#ffff66',
        brightBlue: '#66b3ff',
        brightMagenta: '#ff66ff',
        brightCyan: '#66ffff',
        brightWhite: '#ffffff'
      },
      allowProposedApi: true
    });

    const fitAddon = new FitAddon();
    const webLinksAddon = new WebLinksAddon();

    term.loadAddon(fitAddon);
    term.loadAddon(webLinksAddon);
    term.open(terminalRef.current);
    fitAddon.fit();

    xtermRef.current = term;
    fitAddonRef.current = fitAddon;

    // Handle terminal input - send to WebSocket
    term.onData((data) => {
      if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
        wsRef.current.send(data);
      }
    });

    // Handle resize
    const handleResize = () => {
      if (fitAddonRef.current) {
        fitAddonRef.current.fit();
      }
    };
    window.addEventListener('resize', handleResize);

    return () => {
      window.removeEventListener('resize', handleResize);
      term.dispose();
      xtermRef.current = null;
    };
  }, []);

  // Update terminal theme when global theme changes
  useEffect(() => {
    if (xtermRef.current) {
      xtermRef.current.options.theme = {
        background: '#050505',
        foreground: theme.colors.primary,
        cursor: theme.colors.primary,
        cursorAccent: '#000000',
        selectionBackground: `${theme.colors.primary}4D`,
        black: '#000000',
        red: theme.colors.danger,
        green: theme.colors.success,
        yellow: '#FDFD96',
        blue: theme.colors.secondary,
        magenta: '#A01641',
        cyan: '#00cccc',
        white: '#BEBEBE',
        brightBlack: '#666666',
        brightRed: '#ff6b6b',
        brightGreen: '#33ff33',
        brightYellow: '#ffff66',
        brightBlue: '#66b3ff',
        brightMagenta: '#ff66ff',
        brightCyan: '#66ffff',
        brightWhite: '#ffffff'
      };
    }
  }, [theme]);
  useEffect(() => {
    const resizeObserver = new ResizeObserver(() => {
      if (fitAddonRef.current) {
        fitAddonRef.current.fit();
      }
    });

    if (terminalRef.current) {
      resizeObserver.observe(terminalRef.current);
    }

    return () => resizeObserver.disconnect();
  }, []);

  const generateTerminalId = () => {
    const chars = '0123456789abcdef';
    let result = '';
    for (let i = 0; i < 8; i++) {
      result += chars[Math.floor(Math.random() * chars.length)];
    }
    return result;
  };

  const startTerminal = () => {
    if (status !== 'stopped') return;

    const termProgram = customProgram || program;
    if (!termProgram) {
      xtermRef.current?.writeln('\r\n[Error] No program specified\r\n');
      return;
    }

    setStatus('connecting');
    xtermRef.current?.clear();
    xtermRef.current?.writeln(`[*] Connecting to ${agent.a_computer}...`);
    xtermRef.current?.writeln(`[*] Starting: ${termProgram}`);

    // Build WebSocket URL
    const token = localStorage.getItem('adaptix_token');
    if (!token) {
      xtermRef.current?.writeln('\r\n[-] No auth token found\r\n');
      setStatus('stopped');
      return;
    }

    const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsHost = window.location.host;
    const proxyPath = '/api/proxy/channel'; 
    
    // Build terminal data: agentId|terminalId|program(base64)|sizeH|sizeW|OemCP
    const terminalId = generateTerminalId();
    
    // Qt-compatible program encoding: Base64
    let programB64 = '';
    try {
      // Use TextEncoder to handle potential Unicode characters in custom paths
      const encoder = new TextEncoder();
      const encoded = encoder.encode(termProgram);
      let binary = '';
      for (let i = 0; i < encoded.length; i++) {
        binary += String.fromCharCode(encoded[i]);
      }
      programB64 = btoa(binary);
    } catch (e) {
      programB64 = btoa(termProgram);
    }

    const cols = xtermRef.current?.cols || 80;
    const rows = xtermRef.current?.rows || 24;
    const oemCP = agent.a_oemcp || 0;
    
    const terminalData = btoa(`${agent.a_id}|${terminalId}|${programB64}|${rows}|${cols}|${oemCP}`);

    // Standardized channel URL for Gateway proxy
    const wsUrl = `${wsProtocol}//${wsHost}${proxyPath}?token=${encodeURIComponent(token)}&channel_type=terminal&channel_data=${encodeURIComponent(terminalData)}`;
    
    try {
      const wsAuth = new WebSocket(wsUrl);

      wsAuth.binaryType = 'arraybuffer';

      wsAuth.onopen = () => {
        setStatus('running');
        xtermRef.current?.writeln('[+] Connected!\r\n');
        wsRef.current = wsAuth;
      };

      wsAuth.onmessage = (event) => {
        if (event.data instanceof ArrayBuffer) {
          const decoder = new TextDecoder('utf-8');
          const text = decoder.decode(event.data);
          xtermRef.current?.write(text);
        } else {
          xtermRef.current?.write(event.data);
        }
      };

      wsAuth.onerror = (err) => {
        console.error('[Terminal] WebSocket error:', err);
        xtermRef.current?.writeln('\r\n[-] Connection error\r\n');
        setStatus('stopped');
      };

      wsAuth.onclose = () => {
        xtermRef.current?.writeln('\r\n[*] Connection closed\r\n');
        setStatus('stopped');
        wsRef.current = null;
      };

    } catch (err) {
      console.error('[Terminal] Failed to connect:', err);
      xtermRef.current?.writeln(`\r\n[-] Failed: ${err.message}\r\n`);
      setStatus('stopped');
    }
  };

  const stopTerminal = () => {
    if (wsRef.current) {
      wsRef.current.close();
      wsRef.current = null;
    }
    setStatus('stopped');
  };

  const handleProgramSelect = (prog) => {
    if (prog.path === '') {
      setCustomProgram('');
      setProgram('');
    } else {
      setProgram(prog.path);
      setCustomProgram('');
    }
    setShowProgramMenu(false);
  };

  const selectedProgramLabel = programs.find(p => p.path === program)?.label || 'Custom';

  return (
    <div className="flex flex-col h-full w-full select-none overflow-hidden" onClick={() => setShowProgramMenu(false)}>
      {/* 1. Toolbar */}
      <div className="flex items-center justify-between px-3 py-2 glass-card-sm border-b border-theme-glass-light shrink-0">
        <div className="flex items-center space-x-3">
          {/* Program selector */}
          <div className="relative">
            <button
              onClick={(e) => { e.stopPropagation(); setShowProgramMenu(!showProgramMenu); }}
              disabled={status === 'running'}
              className={cn(
                "glass-btn flex items-center space-x-2 px-4 py-2",
                status === 'running' && "opacity-50 cursor-not-allowed"
              )}
            >
              <span className="font-semibold text-sm text-theme-accent">{selectedProgramLabel}</span>
              <ChevronDown className="w-4 h-4 text-theme-muted" />
            </button>
            
            {showProgramMenu && (
              <div className="absolute top-full left-0 mt-1 glass-panel border border-theme-glass-light rounded-xl shadow-glow z-50 min-w-[180px] overflow-hidden animate-in fade-in zoom-in-95 duration-200">
                <div className="px-4 py-2 bg-theme-glass border-b border-theme-glass-light">
                  <span className="text-[10px] font-black uppercase text-theme-muted tracking-widest">Select Shell</span>
                </div>
                {programs.map((prog) => (
                  <button
                    key={prog.label}
                    onClick={() => handleProgramSelect(prog)}
                    className="w-full px-4 py-3 text-left text-sm font-bold text-theme-secondary hover:bg-theme-glass hover:text-theme-primary transition-all border-b border-theme-glass-light last:border-0 group flex items-center justify-between"
                  >
                    <span>{prog.label}</span>
                    <TerminalIcon size={12} className="text-theme-muted opacity-0 group-hover:opacity-100 transition-opacity" />
                  </button>
                ))}
              </div>
            )}
          </div>

          {/* Custom program input */}
          {program === '' && (
            <div className="flex items-center glass-input rounded-xl px-4 py-2">
              <Settings size={14} className="text-theme-muted mr-2" />
              <input
                type="text"
                value={customProgram}
                onChange={(e) => setCustomProgram(e.target.value)}
                placeholder="Enter shell path..."
                disabled={status === 'running'}
                className="bg-transparent outline-none text-sm font-mono text-theme-primary w-64 placeholder:text-theme-muted"
              />
            </div>
          )}
        </div>

        <div className="flex items-center space-x-3 pr-1">
          {/* Status indicator */}
          <div className={cn(
            "flex items-center space-x-2 px-3 py-1.5 rounded-lg text-[9px] font-black uppercase tracking-[0.2em] border border-theme-glass-light bg-theme-glass-panel",
            status === 'running' ? "text-theme-success" : status === 'connecting' ? "text-theme-accent" : "text-theme-muted"
          )}>
            <div className={cn(
              "w-1.5 h-1.5 rounded-full shadow-glow-sm",
              status === 'running' ? "bg-theme-success" : 
              status === 'connecting' ? "bg-theme-accent animate-pulse" : "bg-theme-muted opacity-40"
            )} />
            <span>CHANNEL_{status.toUpperCase()}</span>
          </div>

          {/* Start/Stop buttons */}
          {status === 'stopped' ? (
            <button
              onClick={startTerminal}
              className="glass-btn-primary px-6 py-2 flex items-center space-x-2 shadow-glow-sm hover:shadow-glow text-white"
            >
              <Play className="w-3 h-3 text-white" />
              <span className="font-black uppercase tracking-widest text-[10px]">Establish</span>
            </button>
          ) : (
            <button
              onClick={stopTerminal}
              className="glass-btn px-6 py-2 border-theme-danger/30 text-theme-danger hover:bg-theme-danger/10 flex items-center space-x-2"
            >
              <Square className="w-3 h-3" fill="currentColor" />
              <span className="font-black uppercase tracking-widest text-[10px]">Terminate</span>
            </button>
          )}
        </div>
      </div>

      {/* Terminal container */}
      <div className="flex-1 p-1 bg-black overflow-hidden relative group/term">
        <div 
          ref={terminalRef} 
          className="w-full h-full"
        />
        {status === 'stopped' && (
          <div className="absolute inset-0 bg-theme-glass-panel/60 backdrop-blur-[2px] flex flex-col items-center justify-center space-y-4 opacity-80 pointer-events-none transition-all">
            <TerminalIcon size={64} className="text-theme-muted/40" strokeWidth={1} />
            <div className="text-center">
              <p className="text-[10px] font-black text-theme-muted uppercase tracking-[0.4em]">Interactive Pseudo-TTY</p>
              <p className="text-[8px] text-theme-accent font-bold mt-1 uppercase tracking-widest">Awaiting initialization command</p>
            </div>
          </div>
        )}
      </div>

      {/* Footer info */}
      <div className="px-3 py-1.5 bg-theme-glass border-t border-theme-glass-light flex items-center justify-between text-[9px] font-black text-theme-muted uppercase tracking-[0.15em] shrink-0">
        <div className="flex items-center space-x-6">
          <div className="flex items-center space-x-2 bg-theme-glass-panel px-3 py-1 rounded-lg border border-theme-glass-light">
            <span className="text-theme-muted opacity-60">TARGET_NODE:</span>
            <span className="text-theme-accent font-mono italic normal-case">{agent.a_computer}</span>
          </div>
          <div className="flex items-center space-x-2 bg-theme-glass-panel px-3 py-1 rounded-lg border border-theme-glass-light">
            <span className="text-theme-muted opacity-60">TTY_EMULATION:</span>
            <span className="text-theme-secondary font-mono tracking-tighter">XTERM_VT100</span>
          </div>
        </div>
        <div className="flex items-center space-x-3">
          <span className="text-theme-muted">ENCRYPTED_STREAM_LINK</span>
          <div className="w-2 h-2 rounded-full bg-theme-success shadow-glow-sm animate-pulse" />
        </div>
      </div>
    </div>
  );
};

export default RemoteTerminal;
