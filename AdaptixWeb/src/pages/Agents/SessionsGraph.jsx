import React, { useMemo, useState } from 'react';
import { useAgents } from '../../context/AgentContext';
import ContextMenu from '../../components/ContextMenu';
import { 
  Monitor, 
  User, 
  Server, 
  ArrowRight,
  Shield,
  Zap,
  Activity,
  Terminal,
  Files,
  Power
} from 'lucide-react';
import { motion, AnimatePresence } from 'framer-motion';
import { cn } from '../../utils/cn';
import { agentApi } from '../../api/agent';

const SessionsGraph = () => {
  const { agents, openAgentTab } = useAgents();
  const [menu, setMenu] = useState(null);

  const handleNodeContextMenu = (e, node) => {
    if (node.type === 'root') return;
    e.preventDefault();
    e.stopPropagation();
    
    setMenu({
      x: e.clientX,
      y: e.clientY,
      options: [
        { label: `Interact (${node.label})`, icon: Terminal, onClick: () => openAgentTab(node.agent, 'console') },
        { label: 'File Browser', icon: Files, onClick: () => openAgentTab(node.agent, 'files') },
        { label: 'Process List', icon: Activity, onClick: () => openAgentTab(node.agent, 'procs') },
        { divider: true },
        { label: 'Exit Agent', icon: Power, color: 'text-theme-danger', onClick: () => {
          if (window.confirm('Terminate this agent?')) {
            agentApi.remove([node.id]);
          }
        }},
      ]
    });
  };

  // Simple tree layout calculation
  const graphData = useMemo(() => {
    const nodes = [];
    const links = [];
    const rootId = 'TEAMSERVER_ROOT';

    // 1. Root Node (Teamserver)
    nodes.push({
      id: rootId,
      type: 'root',
      label: 'Teamserver',
      x: 400,
      y: 50,
      icon: Server,
      color: 'text-theme-accent'
    });

    // 2. Map Agents to levels
    const agentMap = new Map();
    agents.forEach(a => agentMap.set(a.a_id, a));

    const levels = { 0: [rootId] };
    const processed = new Set([rootId]);
    
    // Group agents by parent
    const childrenMap = new Map();
    agents.forEach(a => {
      const parentId = a.a_parent_id || rootId;
      if (!childrenMap.has(parentId)) childrenMap.set(parentId, []);
      childrenMap.get(parentId).push(a.a_id);
    });

    // BFS to assign levels
    let queue = [{ id: rootId, level: 0 }];
    while (queue.length > 0) {
      const item = queue.shift();
      const id = item.id;
      const level = item.level;
      const children = childrenMap.get(id) || [];
      
      if (children.length > 0) {
        if (!levels[level + 1]) levels[level + 1] = [];
        children.forEach(childId => {
          if (!processed.has(childId)) {
            levels[level + 1].push(childId);
            processed.add(childId);
            queue.push({ id: childId, level: level + 1 });
            links.push({ source: id, target: childId });
          }
        });
      }
    }

    // Assign positions based on levels
    Object.keys(levels).forEach(lvlStr => {
      const level = parseInt(lvlStr);
      const levelNodes = levels[level];
      const spacing = 180;
      const startX = 400 - ((levelNodes.length - 1) * spacing) / 2;
      
      levelNodes.forEach((id, idx) => {
        if (id === rootId) return;
        const agent = agentMap.get(id);
        nodes.push({
          id,
          type: 'agent',
          label: agent.a_computer || 'Unknown',
          sublabel: agent.a_username,
          x: startX + idx * spacing,
          y: 50 + level * 150,
          icon: Monitor,
          color: (Math.floor(Date.now() / 1000) - agent.a_last_tick) < 60 ? 'text-theme-accent-secondary' : 'text-theme-muted',
          agent
        });
      });
    });

    return { nodes, links };
  }, [agents]);

  const getNodePos = (id) => {
    const node = graphData.nodes.find(n => n.id === id);
    return node ? { x: node.x, y: node.y } : { x: 0, y: 0 };
  };

  return (
    <div className="w-full h-full bg-theme-glass-panel overflow-hidden relative select-none" onClick={() => setMenu(null)}>
      {/* 1. Header Overlay */}
      <div className="absolute top-4 left-4 z-10 flex flex-col space-y-3">
        <div className="flex items-center space-x-4 px-4 py-2 bg-theme-glass/80 backdrop-blur-md border border-theme-glass-light rounded-xl shadow-glow">
          <Activity size={16} className="text-theme-accent animate-pulse" />
          <div className="flex flex-col text-left">
            <span className="text-[10px] font-black text-theme-primary uppercase tracking-[0.2em]">Infrastructure Topology</span>
            <span className="text-[8px] font-bold text-theme-muted uppercase tracking-widest mt-0.5">Real-time session mapping active</span>
          </div>
        </div>
        
        {/* Legend */}
        <div className="flex flex-col space-y-2 p-3 bg-theme-glass-panel/50 border border-theme-glass-light rounded-xl shadow-sm">
          <div className="flex items-center space-x-2 text-[8px] font-black text-theme-muted uppercase tracking-wider">
            <div className="w-2 h-2 rounded-full bg-theme-accent shadow-glow-sm" />
            <span>Teamserver Control</span>
          </div>
          <div className="flex items-center space-x-2 text-[8px] font-black text-theme-muted uppercase tracking-wider">
            <div className="w-2 h-2 rounded-full bg-theme-accent-secondary shadow-glow-sm" />
            <span>Active Operational Node</span>
          </div>
          <div className="flex items-center space-x-2 text-[8px] font-black text-theme-muted uppercase tracking-wider">
            <div className="w-2 h-2 rounded-full bg-theme-muted opacity-40 shadow-sm" />
            <span>Stale / Inactive Link</span>
          </div>
        </div>
      </div>

      <div className="absolute top-4 right-4 z-10">
        <div className="flex items-center space-x-3 px-3 py-1.5 bg-theme-glass/80 border border-theme-glass-light rounded-xl shadow-sm">
          <span className="text-[10px] font-black text-theme-muted uppercase tracking-widest">Auto_Layout:</span>
          <span className="text-[10px] font-mono text-theme-accent uppercase font-bold">Static_BFS</span>
        </div>
      </div>

      <svg className="w-full h-full cursor-grab active:cursor-grabbing">
        {/* Background Grid Pattern */}
        <defs>
          <pattern id="grid" width="40" height="40" patternUnits="userSpaceOnUse">
            <path d="M 40 0 L 0 0 0 40" fill="none" stroke="rgba(255,255,255,0.02)" strokeWidth="0.5"/>
          </pattern>
          <marker
            id="arrowhead"
            markerWidth="10"
            markerHeight="7"
            refX="25"
            refY="3.5"
            orient="auto"
          >
            <polygon points="0 0, 8 3.5, 0 7" fill="currentColor" className="text-theme-muted opacity-30" />
          </marker>
        </defs>
        <rect width="100%" height="100%" fill="url(#grid)" />

        {/* Links with data-flow particles */}
        {graphData.links.map((link, i) => {
          const start = getNodePos(link.source);
          const end = getNodePos(link.target);
          return (
            <g key={`link-${i}`}>
              <line
                x1={start.x}
                y1={start.y}
                x2={end.x}
                y2={end.y}
                stroke="var(--glass-border-light)"
                strokeWidth="1.5"
                markerEnd="url(#arrowhead)"
              />
              <motion.circle
                r="1.5"
                fill="var(--theme-success)"
                initial={{ offsetDistance: "0%" }}
                animate={{ 
                  cx: [start.x, end.x],
                  cy: [start.y, end.y],
                  opacity: [0, 1, 0]
                }}
                transition={{ 
                  duration: 2.5, 
                  repeat: Infinity, 
                  ease: "linear",
                  delay: i * 0.5
                }}
              />
            </g>
          );
        })}

        {/* Nodes */}
        {graphData.nodes.map((node) => (
          <g key={node.id} transform={`translate(${node.x}, ${node.y})`}>
            <motion.foreignObject
              x="-40"
              y="-40"
              width="80"
              height="100"
              initial={{ scale: 0.8, opacity: 0 }}
              animate={{ scale: 1, opacity: 1 }}
              whileHover={{ scale: 1.05 }}
              onContextMenu={(e) => handleNodeContextMenu(e, node)}
              onDoubleClick={() => node.type === 'agent' && openAgentTab(node.agent, 'console')}
            >
              <div className="flex flex-col items-center group cursor-default">
                <div className={cn(
                  "w-14 h-14 rounded-xl flex items-center justify-center transition-all border-2 relative shadow-glow-sm overflow-hidden",
                  node.type === 'root' 
                    ? "bg-theme-glass-panel border-theme-accent/60 text-theme-accent shadow-glow" 
                    : "bg-theme-glass-panel border-theme-glass-light text-theme-muted group-hover:border-theme-accent/50 group-hover:text-theme-primary",
                  node.color === 'text-theme-accent-secondary' && "border-theme-accent-secondary/60 text-theme-accent-secondary shadow-glow-sm"
                )}>
                  {/* Subtle Scanline Effect */}
                  <div className="absolute inset-0 bg-[linear-gradient(rgba(18,16,16,0)_50%,rgba(0,0,0,0.25)_50%),linear-gradient(90deg,rgba(255,0,0,0.06),rgba(0,255,0,0.02),rgba(0,0,255,0.06))] bg-[size:100%_2px,3px_100%] pointer-events-none opacity-10" />
                  
                  <node.icon size={28} className={cn(
                    "relative z-10",
                    node.color === 'text-theme-accent-secondary' ? 'animate-pulse' : ''
                  )} />
                  
                  {/* Node Status Glow */}
                  {node.color === 'text-theme-accent-secondary' && (
                    <div className="absolute inset-0 bg-theme-accent-secondary/5 animate-pulse" />
                  )}
                </div>
                
                <div className="mt-3 text-center w-full">
                  <div className="bg-theme-glass/80 backdrop-blur-sm px-2.5 py-1 rounded-lg border border-theme-glass-light inline-block max-w-full shadow-sm">
                    <p className="text-[10px] font-black uppercase tracking-widest text-theme-primary truncate leading-none">
                      {node.label}
                    </p>
                  </div>
                  {node.sublabel && (
                    <p className="text-[8px] text-theme-muted font-bold uppercase tracking-widest mt-1 truncate opacity-60">
                      {node.sublabel}
                    </p>
                  )}
                </div>
              </div>
            </motion.foreignObject>
          </g>
        ))}
      </svg>

      {/* Footer Info Overlay */}
      <div className="absolute bottom-4 left-4 z-10">
        <div className="flex items-center space-x-6 px-4 py-2 bg-theme-glass/80 backdrop-blur-md border border-theme-glass-light rounded-xl text-[10px] font-black text-theme-muted uppercase tracking-[0.15em] shadow-glow-sm">
          <div className="flex items-center space-x-2">
            <span className="opacity-60">NODES_ONLINE:</span>
            <span className="text-theme-accent-secondary font-mono">{agents.filter(a => (Math.floor(Date.now() / 1000) - a.a_last_tick) < 300).length}</span>
          </div>
          <div className="w-px h-4 bg-theme-glass-light" />
          <div className="flex items-center space-x-2">
            <span className="opacity-60">TOTAL_LINKS:</span>
            <span className="text-theme-primary font-mono">{graphData.links.length}</span>
          </div>
        </div>
      </div>

      {/* Context Menu */}
      {menu && <ContextMenu {...menu} onClose={() => setMenu(null)} />}
    </div>
  );
};

export default SessionsGraph;
