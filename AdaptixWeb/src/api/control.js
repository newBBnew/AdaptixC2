import api from './agent';

// Helper to prefix proxy routes
const proxy = (url) => `/proxy${url}`;

export const listenerApi = {
  list: () => api.get(proxy('/listener/list')),
  start: (data) => api.post(proxy('/listener/create'), data),
  stop: (name, type) => api.post(proxy('/listener/stop'), { name, type, config: '' }),
  edit: (data) => api.post(proxy('/listener/edit'), data),
};

export const taskApi = {
  list: (agentId) => api.get(proxy(`/agent/task/list?agent_id=${agentId}`)),
  cancel: (agentId, taskIds) => api.post(proxy('/agent/task/cancel'), { agent_id: agentId, tasks_array: taskIds }),
  delete: (agentId, taskIds) => api.post(proxy('/agent/task/delete'), { agent_id: agentId, tasks_array: taskIds }),
};

export const tunnelApi = {
  list: () => api.get(proxy('/tunnel/list')),
  startSocks5: (data) => api.post(proxy('/tunnel/start/socks5'), data),
  startSocks4: (data) => api.post(proxy('/tunnel/start/socks4'), data),
  startLportfwd: (data) => api.post(proxy('/tunnel/start/lportfwd'), data),
  startRportfwd: (data) => api.post(proxy('/tunnel/start/rportfwd'), data),
  stop: (tunnelId) => api.post(proxy('/tunnel/stop'), { p_tunnel_id: tunnelId }),
  setInfo: (tunnelId, info) => api.post(proxy('/tunnel/set/info'), { p_tunnel_id: tunnelId, p_info: info }),
};

export const deliveryApi = {
  list: () => api.get(proxy('/filedelivery/list')),
  stop: (id) => api.post(proxy('/filedelivery/delete'), { file_id: id }),
  upload: (formData, onProgress) => api.post(proxy('/filedelivery/upload'), formData, {
    headers: { 'Content-Type': 'multipart/form-data' },
    onUploadProgress: (progressEvent) => {
      const percentCompleted = Math.round((progressEvent.loaded * 100) / progressEvent.total);
      onProgress?.(percentCompleted);
    }
  }),
  createLink: (data) => api.post(proxy('/filedelivery/link/create'), data),
};

export const scriptApi = {
  // Scripts are managed directly by the Gateway execution environment
  getBasePath: () => api.get('/extensions/metadata').then(res => ({ data: { ok: true, path: res.data.base || './Extension-Kit' } })),
  list: (path = '') => api.get(`/extensions/list?path=${encodeURIComponent(path)}`),
  read: (path) => api.post('/extensions/read', { path }),
  write: (path, content) => api.post('/extensions/write', { path, content }),
  // BOF binary reading still via core if needed, but Gateway can also handle it
  readBof: (path) => api.post(proxy('/script/bof'), { path }),
};

export const dataApi = {
  downloads: () => api.get(proxy('/download/list')),
  targets: () => api.get(proxy('/targets/list')),
  creds: () => api.get(proxy('/creds/list')),
  screenshots: () => api.get(proxy('/screen/list')),
  createTarget: (data) => api.post(proxy('/targets/add'), data),
  editTarget: (data) => api.post(proxy('/targets/edit'), data),
  removeTarget: (ids) => api.post(proxy('/targets/remove'), { target_id_array: ids }),
  setTargetTag: (ids, tag) => api.post(proxy('/targets/set/tag'), { target_id_array: ids, tag }),
  createCred: (data) => api.post(proxy('/creds/add'), data),
  editCred: (data) => api.post(proxy('/creds/edit'), data),
  removeCred: (ids) => api.post(proxy('/creds/remove'), { cred_id_array: ids }),
  setCredTag: (ids, tag) => api.post(proxy('/creds/set/tag'), { cred_id_array: ids, tag }),
  getOTP: (type, id) => api.post(proxy('/otp/generate'), { type, id }),
  setScreenshotNote: (ids, note) => api.post(proxy('/screen/setnote'), { screen_id_array: ids, note }),
  removeScreenshot: (ids) => api.post(proxy('/screen/remove'), { screen_id_array: ids }),
};

export const pivotApi = {
  list: () => api.get(proxy('/pivot/list')),
  remove: (id) => api.post(proxy('/pivot/remove'), { id }),
};
