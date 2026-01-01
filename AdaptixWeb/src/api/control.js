import api from './agent';

export const listenerApi = {
  list: () => api.get('/listener/list'),
  start: (data) => api.post('/listener/create', data),
  stop: (name, type) => api.post('/listener/stop', { name, type }),
};

export const taskApi = {
  list: (agentId) => api.get(`/agent/task/list?agent_id=${agentId}`),
  cancel: (agentId, taskIds) => api.post('/agent/task/cancel', { agent_id: agentId, tasks_array: taskIds }),
  delete: (agentId, taskIds) => api.post('/agent/task/delete', { agent_id: agentId, tasks_array: taskIds }),
};

export const tunnelApi = {
  list: () => api.get('/tunnel/list'),
  stop: (tunnelId) => api.post('/tunnel/stop', { tunnel_id: tunnelId }),
  setInfo: (tunnelId, info) => api.post('/tunnel/set_info', { tunnel_id: tunnelId, info }),
};

export const deliveryApi = {
  list: () => api.get('/file_delivery/list'),
  stop: (id) => api.post('/file_delivery/stop', { id }),
};

export const dataApi = {
  downloads: () => api.get('/data/downloads'),
  targets: () => api.get('/data/targets'),
  creds: () => api.get('/data/creds'),
  screenshots: () => api.get('/data/screenshots'),
};
