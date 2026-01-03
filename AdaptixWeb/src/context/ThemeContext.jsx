import React, { createContext, useContext, useState, useEffect } from 'react';

// Theme definitions
export const themes = {
  glassmorphism: {
    id: 'glassmorphism',
    name: 'Silver Glass',
    description: '银色玻璃拟态风格',
    colors: {
      // Base colors
      background: 'bg-gradient-to-br from-slate-200 via-slate-300 to-slate-400',
      text: 'text-slate-800',
      textMuted: 'text-slate-500',
      // Glass effects
      glassPanel: 'bg-white/25 backdrop-blur-xl border border-white/35',
      glassCard: 'bg-white/20 backdrop-blur-lg border border-white/25',
      glassBtn: 'bg-white/25 backdrop-blur-sm border border-white/35 hover:bg-white/35',
      glassInput: 'bg-white/25 backdrop-blur-sm border border-white/25',
      // Accent colors
      primary: '#3D8B6A',
      secondary: '#60a5fa',
      danger: '#E32227',
      success: '#39FF14',
    }
  },
  dark: {
    id: 'dark',
    name: 'Dark Mode',
    description: '深色主题',
    colors: {
      background: 'bg-dark-950',
      text: 'text-gray-200',
      textMuted: 'text-gray-500',
      glassPanel: 'bg-dark-900/90 backdrop-blur-xl border border-dark-700',
      glassCard: 'bg-dark-800/80 backdrop-blur-lg border border-dark-700',
      glassBtn: 'bg-dark-700/50 backdrop-blur-sm border border-dark-600 hover:bg-dark-600/50',
      glassInput: 'bg-dark-900/50 backdrop-blur-sm border border-dark-700',
      primary: '#3D8B6A',
      secondary: '#89CFF0',
      danger: '#E32227',
      success: '#39FF14',
    }
  },
  cyberpunk: {
    id: 'cyberpunk',
    name: 'Cyberpunk',
    description: '赛博朋克风格',
    colors: {
      background: 'bg-gradient-to-br from-purple-950 via-slate-950 to-cyan-950',
      text: 'text-cyan-100',
      textMuted: 'text-purple-400',
      glassPanel: 'bg-purple-900/30 backdrop-blur-xl border border-cyan-500/30',
      glassCard: 'bg-slate-900/50 backdrop-blur-lg border border-purple-500/30',
      glassBtn: 'bg-cyan-900/30 backdrop-blur-sm border border-cyan-500/40 hover:bg-cyan-800/40',
      glassInput: 'bg-slate-900/40 backdrop-blur-sm border border-purple-500/30',
      primary: '#00FFFF',
      secondary: '#FF00FF',
      danger: '#FF0040',
      success: '#00FF80',
    }
  },
  ocean: {
    id: 'ocean',
    name: 'Ocean Blue',
    description: '海洋蓝主题',
    colors: {
      background: 'bg-gradient-to-br from-blue-100 via-cyan-50 to-teal-100',
      text: 'text-slate-800',
      textMuted: 'text-blue-500',
      glassPanel: 'bg-white/50 backdrop-blur-xl border border-blue-200/50',
      glassCard: 'bg-blue-50/40 backdrop-blur-lg border border-blue-200/40',
      glassBtn: 'bg-blue-100/40 backdrop-blur-sm border border-blue-300/50 hover:bg-blue-200/50',
      glassInput: 'bg-white/40 backdrop-blur-sm border border-blue-200/40',
      primary: '#0077B6',
      secondary: '#00B4D8',
      danger: '#E63946',
      success: '#2A9D8F',
    }
  },
  sunset: {
    id: 'sunset',
    name: 'Sunset Warm',
    description: '日落暖色主题',
    colors: {
      background: 'bg-gradient-to-br from-orange-100 via-rose-50 to-amber-100',
      text: 'text-slate-800',
      textMuted: 'text-orange-600',
      glassPanel: 'bg-white/50 backdrop-blur-xl border border-orange-200/50',
      glassCard: 'bg-orange-50/40 backdrop-blur-lg border border-orange-200/40',
      glassBtn: 'bg-orange-100/40 backdrop-blur-sm border border-orange-300/50 hover:bg-orange-200/50',
      glassInput: 'bg-white/40 backdrop-blur-sm border border-orange-200/40',
      primary: '#F97316',
      secondary: '#FB7185',
      danger: '#DC2626',
      success: '#84CC16',
    }
  },
  darkice: {
    id: 'darkice',
    name: 'Dark Ice',
    description: '暗黑冰冻风格',
    colors: {
      background: 'bg-gradient-to-br from-slate-950 via-black to-slate-950',
      text: 'text-slate-50',
      textMuted: 'text-slate-400',
      glassPanel: 'bg-black/80 backdrop-blur-xl border border-cyan-400/40',
      glassCard: 'bg-slate-950/75 backdrop-blur-lg border border-cyan-400/35',
      glassBtn: 'bg-slate-900/60 backdrop-blur-sm border border-cyan-400/30 hover:bg-slate-800/60',
      glassInput: 'bg-black/60 backdrop-blur-sm border border-cyan-400/25',
      primary: '#22d3ee',
      secondary: '#0ea5e9',
      danger: '#fb7185',
      success: '#34d399',
    }
  }
};

const ThemeContext = createContext();

export const ThemeProvider = ({ children }) => {
  const [currentTheme, setCurrentTheme] = useState(() => {
    const saved = localStorage.getItem('adaptix_theme');
    return saved || 'glassmorphism';
  });

  useEffect(() => {
    localStorage.setItem('adaptix_theme', currentTheme);
    // Apply theme class to document
    document.documentElement.setAttribute('data-theme', currentTheme);
  }, [currentTheme]);

  const theme = themes[currentTheme] || themes.glassmorphism;

  const switchTheme = (themeId) => {
    if (themes[themeId]) {
      setCurrentTheme(themeId);
    }
  };

  return (
    <ThemeContext.Provider value={{ 
      currentTheme, 
      theme, 
      themes, 
      switchTheme,
      themeList: Object.values(themes)
    }}>
      {children}
    </ThemeContext.Provider>
  );
};

export const useTheme = () => {
  const context = useContext(ThemeContext);
  if (!context) {
    throw new Error('useTheme must be used within a ThemeProvider');
  }
  return context;
};

export default ThemeContext;
