import React, { useState, useEffect } from 'react';
import { BrowserRouter as Router, Routes, Route, Navigate } from 'react-router-dom';
import Layout from './components/Layout';
import Dashboard from './pages/Dashboard/Dashboard';
import ControlPlatform from './pages/Agents/Agents';
import Tactical from './pages/Tactical/Tactical';
import Settings from './pages/Settings/Settings';
import LoginPage from './pages/Login/LoginPage';
import { AgentProvider } from './context/AgentContext';
import { SocketProvider } from './context/SocketContext';
import { ThemeProvider } from './context/ThemeContext';
import './App.css'

function App() {
  const [isAuthenticated, setIsAuthenticated] = useState(
    localStorage.getItem('isLoggedIn') === 'true' && !!localStorage.getItem('adaptix_token')
  );

  const handleLoginSuccess = () => {
    localStorage.setItem('isLoggedIn', 'true');
    setIsAuthenticated(true);
  };

  const handleLogout = () => {
    localStorage.removeItem('isLoggedIn');
    setIsAuthenticated(false);
  };

  return (
    <ThemeProvider>
      <SocketProvider>
        <AgentProvider>
          <Router basename="/ui">
            <Routes>
              <Route path="/login" element={
                !isAuthenticated ? (
                  <LoginPage onLogin={handleLoginSuccess} />
                ) : (
                  <Navigate to="/dashboard" replace />
                )
              } />
              
              <Route path="/*" element={
                isAuthenticated ? (
                  <Layout onLogout={handleLogout}>
                    <Routes>
                      <Route path="/" element={<Navigate to="/dashboard" replace />} />
                      <Route path="/dashboard" element={<Dashboard />} />
                      <Route path="/control" element={<ControlPlatform />} />
                      <Route path="/tactical" element={<Tactical />} />
                      <Route path="/settings" element={<Settings />} />
                      <Route path="*" element={<Navigate to="/dashboard" replace />} />
                    </Routes>
                  </Layout>
                ) : (
                  <Navigate to="/login" replace />
                )
              } />
            </Routes>
          </Router>
        </AgentProvider>
      </SocketProvider>
    </ThemeProvider>
  );
}

export default App;
