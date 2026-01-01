import React, { useState } from 'react';
import { ShieldAlert, Lock, User, Globe } from 'lucide-react';
import axios from 'axios';

const LoginPage = ({ onLogin }) => {
  const [formData, setFormData] = useState({
    url: 'http://localhost:443',
    username: '',
    password: '',
  });
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');

  const handleLogin = async (e) => {
    e.preventDefault();
    setLoading(true);
    setError('');
    
    try {
      const response = await axios.post(`/login`, {
        username: formData.username,
        password: formData.password,
        version: 'v1.0' // Match SMALL_VERSION in main.h
      });
      
      if (response.data && response.data.access_token) {
        localStorage.setItem('adaptix_url', formData.url);
        localStorage.setItem('adaptix_user', formData.username);
        localStorage.setItem('adaptix_token', response.data.access_token);
        localStorage.setItem('adaptix_refresh_token', response.data.refresh_token);
        onLogin(true);
      } else {
        setError('Invalid response from server');
      }
    } catch (err) {
      console.error('Login error:', err);
      if (err.response) {
        setError(err.response.data?.message || `Error: ${err.response.status}`);
      } else if (err.request) {
        setError('No response from Teamserver. Check URL and CORS.');
      } else {
        setError('Request failed. Check console for details.');
      }
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="min-h-screen bg-dark-900 flex items-center justify-center p-4">
      <div className="max-w-md w-full space-y-8 bg-dark-800 p-10 rounded-2xl border border-dark-700 shadow-2xl">
        <div className="text-center">
          <div className="flex justify-center">
            <ShieldAlert className="w-16 h-16 text-accent-primary animate-pulse" />
          </div>
          <h2 className="mt-6 text-3xl font-extrabold text-white tracking-tight">Adaptix C2</h2>
          <p className="mt-2 text-sm text-gray-500 font-medium uppercase tracking-widest">Teamserver Authentication</p>
        </div>
        
        <form className="mt-8 space-y-6" onSubmit={handleLogin}>
          <div className="rounded-md shadow-sm space-y-4">
            <div className="relative">
              <div className="absolute inset-y-0 left-0 pl-3 flex items-center pointer-events-none">
                <Globe className="h-5 w-5 text-gray-500" />
              </div>
              <input
                type="text"
                required
                className="bg-dark-900 border border-dark-700 text-white text-sm rounded-lg focus:ring-accent-primary focus:border-accent-primary block w-full pl-10 p-3 outline-none transition-all"
                placeholder="Teamserver URL"
                value={formData.url}
                onChange={(e) => setFormData({ ...formData, url: e.target.value })}
              />
            </div>
            <div className="relative">
              <div className="absolute inset-y-0 left-0 pl-3 flex items-center pointer-events-none">
                <User className="h-5 w-5 text-gray-500" />
              </div>
              <input
                type="text"
                required
                className="bg-dark-900 border border-dark-700 text-white text-sm rounded-lg focus:ring-accent-primary focus:border-accent-primary block w-full pl-10 p-3 outline-none transition-all"
                placeholder="Username"
                value={formData.username}
                onChange={(e) => setFormData({ ...formData, username: e.target.value })}
              />
            </div>
            <div className="relative">
              <div className="absolute inset-y-0 left-0 pl-3 flex items-center pointer-events-none">
                <Lock className="h-5 w-5 text-gray-500" />
              </div>
              <input
                type="password"
                required
                className="bg-dark-900 border border-dark-700 text-white text-sm rounded-lg focus:ring-accent-primary focus:border-accent-primary block w-full pl-10 p-3 outline-none transition-all"
                placeholder="Password"
                value={formData.password}
                onChange={(e) => setFormData({ ...formData, password: e.target.value })}
              />
            </div>
          </div>

          {error && (
            <div className="text-accent-danger text-xs bg-accent-danger/10 p-3 rounded-lg border border-accent-danger/20">
              {error}
            </div>
          )}

          <div>
            <button
              type="submit"
              disabled={loading}
              className="group relative w-full flex justify-center py-3 px-4 border border-transparent text-sm font-bold rounded-lg text-white bg-accent-primary hover:bg-blue-600 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-accent-primary transition-all disabled:opacity-50"
            >
              {loading ? (
                <div className="w-5 h-5 border-2 border-white/30 border-t-white rounded-full animate-spin" />
              ) : (
                'ESTABLISH CONNECTION'
              )}
            </button>
          </div>
        </form>
        
        <div className="pt-6 text-center">
          <p className="text-[10px] text-gray-600 font-mono">
            &copy; 2026 Adaptix Framework v1.0.0-Web
          </p>
        </div>
      </div>
    </div>
  );
};

export default LoginPage;
