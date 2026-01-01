import React from 'react';

const Dashboard = () => {
  return (
    <div className="p-8 space-y-6">
      <header>
        <h1 className="text-2xl font-bold text-white">System Overview</h1>
        <p className="text-gray-400">Real-time status of your operations</p>
      </header>

      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
        {[
          { label: 'Active Agents', value: '12', color: 'text-accent-primary' },
          { label: 'Total Targets', value: '48', color: 'text-accent-secondary' },
          { label: 'Tasks Running', value: '5', color: 'text-accent-warning' },
          { label: 'Critical Alerts', value: '0', color: 'text-accent-danger' },
        ].map((stat) => (
          <div key={stat.label} className="bg-dark-800 p-6 rounded-xl border border-dark-700 shadow-lg">
            <p className="text-sm text-gray-500 font-medium uppercase tracking-wider">{stat.label}</p>
            <p className={`text-3xl font-bold mt-2 ${stat.color}`}>{stat.value}</p>
          </div>
        ))}
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        <div className="lg:col-span-2 bg-dark-800 rounded-xl border border-dark-700 h-96 flex items-center justify-center">
          <p className="text-gray-600">Session Activity Chart (Coming Soon)</p>
        </div>
        <div className="bg-dark-800 rounded-xl border border-dark-700 p-6">
          <h3 className="text-lg font-semibold mb-4 text-white">Recent Events</h3>
          <div className="space-y-4">
            {[1, 2, 3, 4, 5].map((i) => (
              <div key={i} className="flex space-x-3 text-sm">
                <div className="w-1 bg-accent-primary rounded-full" />
                <div>
                  <p className="text-gray-300">Agent [ABC-123] connected from 10.0.0.5</p>
                  <p className="text-[10px] text-gray-600 mt-1">2 minutes ago</p>
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
};

export default Dashboard;
