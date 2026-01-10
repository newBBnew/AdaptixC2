import axios from 'axios';

// Base API pointing to the Gateway
const api = axios.create({
  baseURL: '/api', 
  timeout: 10000,
});

api.interceptors.request.use((config) => {
  const token = localStorage.getItem('adaptix_token');
  if (token) {
    config.headers.Authorization = `Bearer ${token}`;
  }
  return config;
});

api.interceptors.response.use(
  (response) => response,
  (error) => {
    if (error.response) {
      const { status, data } = error.response;
      
      // Handle session expiry
      if (status === 401) {
        // Clear all auth state to prevent infinite redirect loops
        localStorage.removeItem('adaptix_token');
        localStorage.removeItem('adaptix_refresh_token');
        localStorage.removeItem('adaptix_user');
        localStorage.removeItem('isLoggedIn'); // Critical: prevents App.jsx from redirecting back to dashboard
        
        if (!window.location.pathname.includes('/ui/login')) {
          window.location.href = '/ui/login';
        }
      }
      
      // User-friendly error message for common server errors
      if (status >= 500) {
        console.error('[API Error] Server issue:', data?.message || error.message);
      }
    } else if (error.request) {
      console.error('[API Error] Network unreachable');
    }
    
    return Promise.reject(error);
  }
);

// Helper to prefix proxy routes
const proxy = (url) => `/proxy${url}`;

export const agentApi = {
  list: () => api.get(proxy('/agent/list')),
  get: (id) => api.get(proxy(`/agent/info?id=${id}`)),
  generate: (data) => api.post(proxy('/agent/generate'), data),
  remove: (ids) => api.post(proxy('/agent/remove'), { agent_id_array: ids }),
  
  // Command execution goes through the Gateway Interceptor first
  executeCommand: (data) => {
    // Extract command name for Gateway hook lookup
    const cmdline = data.cmdline || '';
    const parts = cmdline.trim().split(/\s+/);
    const cmdName = parts.length > 0 ? parts[0].toLowerCase() : '';
    
    return api.post('/agent/command/execute', {
      ...data,
      command: cmdName // Add dedicated command field for Gateway hooks
    });
  },
  
  setMark: (ids, mark) => api.post(proxy('/agent/set/mark'), { agent_id_array: ids, mark }),
  setTag: (ids, tag) => api.post(proxy('/agent/set/tag'), { agent_id_array: ids, tag }),
  removeConsole: (ids) => api.post(proxy('/agent/console/remove'), { agent_id_array: ids }),
  updateData: (data) => api.post(proxy('/agent/update/data'), data),
  setColor: (ids, bc, fc, reset = false) => api.post(proxy('/agent/set/color'), { agent_id_array: ids, bc, fc, reset }),
  
  // Browser Specific (Now using command execution for compatibility with C2 Core)
  getDisks: (agentId, agentName) => agentApi.executeCommand({
    id: agentId,
    name: agentName,
    cmdline: "disks",
    data: {},
    ui: true
  }),
  listDir: (agentId, agentName, path) => agentApi.executeCommand({
    id: agentId,
    name: agentName,
    cmdline: `ls "${path}"`,
    data: { path: path },
    ui: true
  }),
  getProcesses: (agentId, agentName) => agentApi.executeCommand({
    id: agentId,
    name: agentName,
    cmdline: "ps list",
    data: {},
    ui: true
  }),
  
  // File operations
  uploadFile: (agentId, agentName, localPath, remotePath) => agentApi.executeCommand({
    id: agentId,
    name: agentName,
    cmdline: `upload "${localPath}" "${remotePath}"`,
    data: { src: localPath, dst: remotePath },
    ui: true
  }),
  downloadFile: (agentId, agentName, path) => agentApi.executeCommand({
    id: agentId,
    name: agentName,
    cmdline: `download "${path}"`,
    data: { path: path },
    ui: true
  }),
  deleteFile: (agentId, agentName, path) => agentApi.executeCommand({
    id: agentId,
    name: agentName,
    cmdline: `rm "${path}"`,
    data: { path: path },
    ui: true
  }),
  moveFile: (agentId, agentName, oldPath, newPath) => agentApi.executeCommand({
    id: agentId,
    name: agentName,
    cmdline: `mv "${oldPath}" "${newPath}"`,
    data: { src: oldPath, dst: newPath },
    ui: true
  }),
  copyFile: (agentId, agentName, srcPath, dstPath) => agentApi.executeCommand({
    id: agentId,
    name: agentName,
    cmdline: `cp "${srcPath}" "${dstPath}"`,
    data: { src: srcPath, dst: dstPath },
    ui: true
  }),
  makeDirectory: (agentId, agentName, path) => agentApi.executeCommand({
    id: agentId,
    name: agentName,
    cmdline: `mkdir "${path}"`,
    data: { path: path },
    ui: true
  }),
  killProcess: (agentId, agentName, pid) => agentApi.executeCommand({
    id: agentId,
    name: agentName,
    cmdline: `ps kill ${pid}`,
    data: { pid: parseInt(pid) },
    ui: true
  }),
  injectProcess: (agentId, agentName, pid, listenerName) => agentApi.executeCommand({
    id: agentId,
    name: agentName,
    cmdline: `inject ${pid} "${listenerName}"`,
    data: { pid: parseInt(pid), listener: listenerName },
    ui: true
  }),
};

export default api;
