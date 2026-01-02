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
  targets: () => api.get('/target/list'),
  creds: () => api.get('/credential/list'),
  screenshots: () => api.get('/screenshot/list'),
  createTarget: (data) => api.post('/target/create', data),
  editTarget: (data) => api.post('/target/edit', data),
  removeTarget: (ids) => api.post('/target/delete', { target_id_array: ids }),
  createCred: (data) => api.post('/credential/create', data),
  editCred: (data) => api.post('/credential/edit', data),
  removeCred: (ids) => api.post('/credential/delete', { cred_id_array: ids }),
  getOTP: (type, id) => api.post('/otp/generate', { type, id }),
  setScreenshotNote: (ids, note) => api.post('/screenshot/set_note', { screen_id_array: ids, note }),
  removeScreenshot: (ids) => api.post('/screenshot/delete', { screen_id_array: ids }),
};
