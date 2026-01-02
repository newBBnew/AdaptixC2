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

const RemoteTerminal = ({ agent }) => {
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
      fontSize: 13,
      fontFamily: '"JetBrains Mono", "Fira Code", monospace',
      theme: {
        background: '#0d0d0d',
        foreground: '#e0e0e0',
        cursor: '#00ff00',
        cursorAccent: '#000000',
        selectionBackground: 'rgba(255, 255, 255, 0.2)',
        black: '#000000',
        red: '#e34234',
        green: '#00ff00',
        yellow: '#ffcc00',
        blue: '#0099ff',
        magenta: '#cc00ff',
        cyan: '#00cccc',
        white: '#ffffff',
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

  // Resize terminal when container size changes
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
    const endpoint = '/api'; // Vite proxy will handle this
    
    // Build terminal data: agentId|terminalId|program(base64)|sizeH|sizeW|OemCP
    const terminalId = generateTerminalId();
    const programB64 = btoa(termProgram);
    const cols = xtermRef.current?.cols || 80;
    const rows = xtermRef.current?.rows || 24;
    const oemCP = agent.a_oemcp || 0;
    
    const terminalData = btoa(`${agent.a_id}|${terminalId}|${programB64}|${rows}|${cols}|${oemCP}`);

    // Note: Browser WebSocket doesn't support custom headers
    // We need to pass auth via query params
    const wsUrl = `${wsProtocol}//${wsHost}${endpoint}/channel?token=${encodeURIComponent(token)}&channel_type=terminal&channel_data=${encodeURIComponent(terminalData)}`;
    
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
    <div className="flex flex-col h-full w-full bg-dark-900">
      {/* Toolbar */}
      <div className="flex items-center justify-between px-3 py-2 bg-dark-800 border-b border-dark-700 shrink-0">
        <div className="flex items-center space-x-3">
          <div className="flex items-center space-x-2 px-2 py-0.5 rounded bg-accent-primary/10 border border-accent-primary/20">
            <TerminalIcon className="w-3.5 h-3.5 text-accent-primary" />
            <span className="text-[10px] font-black uppercase tracking-widest text-accent-primary">Terminal</span>
          </div>

          {/* Program selector */}
          <div className="relative">
            <button
              onClick={() => setShowProgramMenu(!showProgramMenu)}
              disabled={status === 'running'}
              className={cn(
                "flex items-center space-x-2 px-3 py-1.5 rounded text-sm",
                status === 'running' 
                  ? "bg-dark-700 text-gray-500 cursor-not-allowed"
                  : "bg-dark-700 text-gray-300 hover:bg-dark-600"
              )}
            >
              <span>{selectedProgramLabel}</span>
              <ChevronDown className="w-3 h-3" />
            </button>
            
            {showProgramMenu && (
              <div className="absolute top-full left-0 mt-1 bg-dark-800 border border-dark-600 rounded shadow-lg z-10 min-w-[150px]">
                {programs.map((prog) => (
                  <button
                    key={prog.label}
                    onClick={() => handleProgramSelect(prog)}
                    className="w-full px-3 py-2 text-left text-sm text-gray-300 hover:bg-dark-700"
                  >
                    {prog.label}
                  </button>
                ))}
              </div>
            )}
          </div>

          {/* Custom program input */}
          {program === '' && (
            <input
              type="text"
              value={customProgram}
              onChange={(e) => setCustomProgram(e.target.value)}
              placeholder="Enter program path..."
              disabled={status === 'running'}
              className="px-3 py-1.5 bg-dark-950 border border-dark-600 rounded text-sm text-white w-64 outline-none focus:border-accent-primary"
            />
          )}
        </div>

        <div className="flex items-center space-x-2">
          {/* Status indicator */}
          <div className={cn(
            "flex items-center space-x-1.5 px-2 py-1 rounded text-[10px] font-bold uppercase",
            status === 'running' && "bg-green-500/20 text-green-400",
            status === 'connecting' && "bg-yellow-500/20 text-yellow-400",
            status === 'stopped' && "bg-gray-500/20 text-gray-400"
          )}>
            <div className={cn(
              "w-1.5 h-1.5 rounded-full",
              status === 'running' && "bg-green-400 animate-pulse",
              status === 'connecting' && "bg-yellow-400 animate-pulse",
              status === 'stopped' && "bg-gray-400"
            )} />
            <span>{status}</span>
          </div>

          {/* Start/Stop buttons */}
          {status === 'stopped' ? (
            <button
              onClick={startTerminal}
              className="flex items-center space-x-1.5 px-3 py-1.5 bg-green-600 hover:bg-green-500 text-white rounded text-sm transition-colors"
            >
              <Play className="w-3.5 h-3.5" />
              <span>Start</span>
            </button>
          ) : (
            <button
              onClick={stopTerminal}
              className="flex items-center space-x-1.5 px-3 py-1.5 bg-red-600 hover:bg-red-500 text-white rounded text-sm transition-colors"
            >
              <Square className="w-3.5 h-3.5" />
              <span>Stop</span>
            </button>
          )}
        </div>
      </div>

      {/* Terminal container */}
      <div 
        ref={terminalRef} 
        className="flex-1 p-2 overflow-hidden"
        onClick={() => setShowProgramMenu(false)}
      />
    </div>
  );
};

export default RemoteTerminal;
