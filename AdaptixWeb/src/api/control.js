import api from './agent';

export const listenerApi = {
  list: () => api.get('/listener/list'),
  start: (data) => api.post('/listener/create', data),
  stop: (name, type) => api.post('/listener/stop', { name, type, config: '' }),
  edit: (data) => api.post('/listener/edit', data),
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
  upload: (formData) => api.post('/file_delivery/upload', formData, {
    headers: { 'Content-Type': 'multipart/form-data' }
  }),
  createLink: (data) => api.post('/file_delivery/link/create', data),
};

export const scriptApi = {
  getBasePath: () => api.get('/script/basepath'),
  list: (path = '') => api.get(`/script/list?path=${encodeURIComponent(path)}`),
  read: (path) => api.post('/script/read', { path }),
  readBof: (path) => api.post('/script/bof', { path }),
};

export const dataApi = {
  downloads: () => api.get('/download/list'),
  targets: () => api.get('/targets/list'),
  creds: () => api.get('/creds/list'),
  screenshots: () => api.get('/screen/list'),
  createTarget: (data) => api.post('/targets/add', data),
  editTarget: (data) => api.post('/targets/edit', data),
  removeTarget: (ids) => api.post('/targets/remove', { target_id_array: ids }),
  createCred: (data) => api.post('/creds/add', data),
  editCred: (data) => api.post('/creds/edit', data),
  removeCred: (ids) => api.post('/creds/remove', { cred_id_array: ids }),
  getOTP: (type, id) => api.post('/otp/generate', { type, id }),
  setScreenshotNote: (ids, note) => api.post('/screen/setnote', { screen_id_array: ids, note }),
  removeScreenshot: (ids) => api.post('/screen/remove', { screen_id_array: ids }),
};
