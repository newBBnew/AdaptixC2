import axios from 'axios';

const api = axios.create({
  timeout: 10000,
});

// Update baseURL dynamically from localStorage or context if available
api.interceptors.request.use((config) => {
  const savedConfig = localStorage.getItem('adaptix_config');
  if (savedConfig) {
    const { apiEndpoint, teamserverUrl } = JSON.parse(savedConfig);
    
    // If the target is localhost or 127.0.0.1, we should use relative paths
    // to trigger the Vite proxy and avoid CORS issues in development.
    const isLocal = teamserverUrl && (teamserverUrl.includes('localhost') || teamserverUrl.includes('127.0.0.1'));
    
    if (isLocal) {
      // Use the stored apiEndpoint which now includes /api + prefix (e.g. /api/endpoint)
      config.baseURL = apiEndpoint || '/api';
    } else if (apiEndpoint && !config.url.startsWith('http')) {
      config.baseURL = apiEndpoint;
    }
  } else {
    // Fallback to proxy if no dynamic config exists
    config.baseURL = '/api';
  }

  const token = localStorage.getItem('adaptix_token');
  if (token) {
    config.headers.Authorization = `Bearer ${token}`;
  }
  return config;
});

export const agentApi = {
  list: () => api.get('/agent/list'),
  get: (id) => api.get(`/agent/info?id=${id}`),
  generate: (data) => api.post('/agent/generate', data),
  remove: (ids) => api.post('/agent/remove', { agent_id_array: ids }),
  executeCommand: (data) => api.post('/agent/command/execute', data),
  setMark: (ids, mark) => api.post('/agent/set/mark', { agent_id_array: ids, mark }),
  setTag: (ids, tag) => api.post('/agent/set/tag', { agent_id_array: ids, tag }),
  removeConsole: (ids) => api.post('/agent/console/remove', { agent_id_array: ids }),
  updateData: (data) => api.post('/agent/update/data', data),
  setColor: (ids, bc, fc, reset = false) => api.post('/agent/set/color', { agent_id_array: ids, bc, fc, reset }),
  // Browser Specific
  getDisks: (agentId) => api.post('/agent/browser/disks', { id: agentId }),
  listDir: (agentId, path) => api.post('/agent/browser/list', { id: agentId, b_path: path }),
  getProcesses: (agentId) => api.post('/agent/browser/process', { id: agentId }),
};

export default api;
