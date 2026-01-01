import React from 'react';

const Settings = () => {
  return (
    <div className="p-8 space-y-8 max-w-4xl">
      <header>
        <h1 className="text-2xl font-bold text-white">System Settings</h1>
        <p className="text-gray-400">Configure UI preferences and core engine parameters</p>
      </header>

      <section className="space-y-6">
        <div className="bg-dark-800 rounded-xl border border-dark-700 p-6 space-y-6">
          <h2 className="text-lg font-semibold text-accent-primary border-b border-dark-700 pb-4">Theme & Appearance</h2>
          <div className="grid grid-cols-1 md:grid-cols-2 gap-8">
            <div className="space-y-2">
              <label className="text-sm font-medium text-gray-400">Primary Theme</label>
              <div className="flex space-x-4">
                <button className="flex-1 py-3 bg-dark-900 border-2 border-accent-primary rounded-lg text-white font-medium text-sm">Midnight Black</button>
                <button className="flex-1 py-3 bg-dark-700 border border-dark-600 rounded-lg text-gray-400 font-medium text-sm opacity-50 cursor-not-allowed">Cyber Dark (TBA)</button>
              </div>
            </div>
            <div className="space-y-2">
              <label className="text-sm font-medium text-gray-400">Console Font Size</label>
              <select className="w-full bg-dark-900 border border-dark-700 rounded-lg px-4 py-3 text-sm focus:outline-none focus:ring-2 focus:ring-accent-primary">
                <option>12px - Minimal</option>
                <option selected>14px - Standard</option>
                <option>16px - High Legibility</option>
              </select>
            </div>
          </div>
        </div>

        <div className="bg-dark-800 rounded-xl border border-dark-700 p-6 space-y-6">
          <h2 className="text-lg font-semibold text-accent-secondary border-b border-dark-700 pb-4">Engine (MCP) Configuration</h2>
          <div className="space-y-4">
            <div className="flex items-center justify-between p-4 bg-dark-900 rounded-lg">
              <div>
                <p className="text-sm font-medium text-white">Auto-Command Execution</p>
                <p className="text-xs text-gray-500">Allow AI to execute basic recon automatically upon agent check-in</p>
              </div>
              <div className="w-12 h-6 bg-accent-secondary rounded-full flex items-center px-1">
                <div className="w-4 h-4 bg-white rounded-full translate-x-6" />
              </div>
            </div>
            <div className="flex items-center justify-between p-4 bg-dark-900 rounded-lg opacity-50">
              <div>
                <p className="text-sm font-medium text-white">Cloud Extension Loading</p>
                <p className="text-xs text-gray-500">Enable loading Extension-Kit from Teamserver</p>
              </div>
              <div className="w-12 h-6 bg-dark-600 rounded-full flex items-center px-1">
                <div className="w-4 h-4 bg-gray-400 rounded-full" />
              </div>
            </div>
          </div>
        </div>
      </section>
    </div>
  );
};

export default Settings;
