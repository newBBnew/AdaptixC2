import React, { createContext, useContext, useState } from 'react';

const AgentContext = createContext();

export const useAgents = () => useContext(AgentContext);

export const AgentProvider = ({ children }) => {
  const [openTabs, setOpenTabs] = useState([]);
  const [activeTabId, setActiveTabId] = useState(null);

  const openAgentTab = (agent, subTab = 'console') => {
    const agentId = agent.a_id;
    const existingTab = openTabs.find(t => t.a_id === agentId);
    if (!existingTab) {
      setOpenTabs([...openTabs, { ...agent, type: 'agent', activeSubTab: subTab }]);
    } else if (subTab !== existingTab.activeSubTab) {
      setOpenTabs(openTabs.map(t => t.a_id === agentId ? { ...t, activeSubTab: subTab } : t));
    }
    setActiveTabId(agentId);
  };

  const setActiveSubTab = (agentId, subTab) => {
    setOpenTabs(openTabs.map(t => t.a_id === agentId ? { ...t, activeSubTab: subTab } : t));
  };

  const closeTab = (id) => {
    const newTabs = openTabs.filter(t => t.a_id !== id);
    setOpenTabs(newTabs);
    if (activeTabId === id && newTabs.length > 0) {
      setActiveTabId(newTabs[newTabs.length - 1].a_id);
    } else if (newTabs.length === 0) {
      setActiveTabId(null);
    }
  };

  return (
    <AgentContext.Provider value={{ 
      openTabs, 
      activeTabId, 
      setActiveTabId, 
      openAgentTab, 
      closeTab 
    }}>
      {children}
    </AgentContext.Provider>
  );
};
