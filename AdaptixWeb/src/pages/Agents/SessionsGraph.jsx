import React, { useMemo } from 'react';
import { useAgents } from '../../context/AgentContext';
import { 
  Monitor, 
  User, 
  Server, 
  ArrowRight,
  Shield,
  Zap,
  Activity
} from 'lucide-react';
import { motion } from 'framer-motion';
import { cn } from '../../utils/cn';

const SessionsGraph = () => {
  const { agents } = useAgents();

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
      color: 'text-accent-primary'
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
      const { id, level } = queue.shift();
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
          color: (Math.floor(Date.now() / 1000) - agent.a_last_tick) < 60 ? 'text-accent-secondary' : 'text-gray-500',
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
    <div className="w-full h-full bg-[#0a0a0a] overflow-auto custom-scrollbar relative select-none">
      <div className="absolute top-4 left-4 z-10 flex flex-col space-y-2">
        <div className="flex items-center space-x-2 px-3 py-1 bg-dark-800/80 border border-dark-700 rounded text-[10px] font-black uppercase tracking-widest text-gray-400">
          <Activity className="w-3 h-3 text-accent-primary" />
          <span>Network Topology</span>
        </div>
      </div>

      <svg className="w-full h-full min-w-[800px] min-h-[600px]">
        <defs>
          <marker
            id="arrowhead"
            markerWidth="10"
            markerHeight="7"
            refX="9"
            refY="3.5"
            orient="auto"
          >
            <polygon points="0 0, 10 3.5, 0 7" fill="#333" />
          </marker>
        </defs>

        {/* Links */}
        {graphData.links.map((link, i) => {
          const start = getNodePos(link.source);
          const end = getNodePos(link.target);
          return (
            <g key={i}>
              <line
                x1={start.x}
                y1={start.y}
                x2={end.x}
                y2={end.y}
                stroke="#333"
                strokeWidth="1.5"
                markerEnd="url(#arrowhead)"
              />
              <motion.circle
                r="3"
                fill="#10b981"
                initial={{ offsetDistance: "0%" }}
                animate={{ 
                  cx: [start.x, end.x],
                  cy: [start.y, end.y],
                  opacity: [0, 1, 0]
                }}
                transition={{ 
                  duration: 2, 
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
          <g key={node.id} transform={`translate(${node.x - 40}, ${node.y - 40})`}>
            <motion.foreignObject
              width="80"
              height="100"
              initial={{ scale: 0.8, opacity: 0 }}
              animate={{ scale: 1, opacity: 1 }}
              whileHover={{ scale: 1.05 }}
            >
              <div className="flex flex-col items-center group cursor-pointer">
                <div className={cn(
                  "w-12 h-12 rounded-xl flex items-center justify-center transition-all border shadow-lg",
                  node.type === 'root' 
                    ? "bg-accent-primary/10 border-accent-primary/30 text-accent-primary" 
                    : "bg-dark-800 border-dark-700 text-gray-400 group-hover:border-accent-primary/50 group-hover:text-accent-primary",
                  node.color && !node.type === 'root' && node.color
                )}>
                  <node.icon size={24} className={node.type === 'agent' && node.color === 'text-accent-secondary' ? 'animate-pulse' : ''} />
                </div>
                <div className="mt-2 text-center">
                  <p className="text-[10px] font-black uppercase tracking-tighter text-white truncate max-w-[70px]">
                    {node.label}
                  </p>
                  {node.sublabel && (
                    <p className="text-[8px] text-gray-500 font-bold truncate max-w-[70px]">
                      {node.sublabel}
                    </p>
                  )}
                </div>
              </div>
            </motion.foreignObject>
          </g>
        ))}
      </svg>
    </div>
  );
};

export default SessionsGraph;
