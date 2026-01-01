import React from 'react';

const Tactical = () => {
  return (
    <div className="flex flex-col h-full">
      <header className="p-8 border-b border-dark-700">
        <h1 className="text-2xl font-bold text-white">Tactical Center</h1>
        <p className="text-gray-400">Orchestrate attacks and manage MSF integration</p>
      </header>
      
      <div className="flex-1 p-8 grid grid-cols-1 lg:grid-cols-4 gap-6">
        <div className="lg:col-span-3 bg-dark-800 rounded-xl border border-dashed border-dark-600 flex flex-col items-center justify-center p-12 text-center">
          <div className="w-20 h-20 bg-dark-700 rounded-full flex items-center justify-center mb-6">
            <p className="text-4xl text-accent-primary font-bold">M</p>
          </div>
          <h2 className="text-xl font-semibold text-white mb-2">Tactical Flow Canvas</h2>
          <p className="text-gray-500 max-w-sm">
            Connect MSF modules and Adaptix commands into a unified workflow. Drag and drop nodes to begin.
          </p>
          <button className="mt-8 px-6 py-2 bg-accent-primary hover:bg-blue-600 text-white rounded-lg transition-colors font-medium">
            Open MSF Module Browser
          </button>
        </div>

        <div className="bg-dark-800 rounded-xl border border-dark-700 p-6 overflow-hidden flex flex-col">
          <h3 className="text-lg font-semibold mb-4 text-white">Command Library</h3>
          <div className="space-y-2 overflow-y-auto pr-2">
            {['Basic Recon', 'PrivEsc Toolkit', 'Lateral Movement', 'Data Exfil'].map((category) => (
              <div key={category} className="p-3 bg-dark-700 rounded-lg hover:border-accent-primary border border-transparent transition-all cursor-pointer">
                <p className="text-sm font-medium text-gray-200">{category}</p>
                <p className="text-[10px] text-gray-500 mt-1">12 commands available</p>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
};

export default Tactical;
