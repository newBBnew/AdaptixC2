import axios from 'axios';

const api = axios.create({
  baseURL: import.meta.env.VITE_API_BASE_URL || '/api',
  timeout: 10000,
});

// 拦截器可在此添加认证 Token
api.interceptors.request.use((config) => {
  const token = localStorage.getItem('adaptix_token');
  if (token) {
    config.headers.Authorization = `Bearer ${token}`;
  }
  return config;
});

export const agentApi = {
  list: () => api.get('/agent/list'),
  get: (id) => api.get(`/agent/info?id=${id}`),
};

export default api;
