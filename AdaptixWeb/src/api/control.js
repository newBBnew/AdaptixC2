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
