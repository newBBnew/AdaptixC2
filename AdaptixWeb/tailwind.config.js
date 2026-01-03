/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        theme: {
          primary: 'var(--text-primary)',
          secondary: 'var(--text-secondary)',
          muted: 'var(--text-muted)',
          accent: 'var(--accent-primary)',
          'accent-secondary': 'var(--accent-secondary)',
          danger: 'var(--accent-danger)',
          success: 'var(--accent-success)',
          warning: 'var(--accent-warning)',
          glass: 'var(--glass-bg)',
          'glass-sm': 'var(--glass-bg-sm)',
          'glass-panel': 'var(--glass-bg-panel)',
          hover: 'var(--glass-hover)',
          'glass-light': 'var(--glass-border-light)',
        },
        dark: {
          950: '#1a1a2e',
          900: '#16213e',
          800: '#0f3460',
          700: '#533483',
          600: '#e94560',
        },
        accent: {
          primary: '#6366f1',    // Indigo
          secondary: '#a855f7',  // Purple
          tertiary: '#06b6d4',   // Cyan
          danger: '#ef4444',
          warning: '#f59e0b',
          success: '#10b981',
          selection: 'rgba(99, 102, 241, 0.2)',
        },
        glass: {
          white: 'rgba(255, 255, 255, 0.1)',
          border: 'rgba(255, 255, 255, 0.2)',
          hover: 'rgba(255, 255, 255, 0.15)',
          active: 'rgba(99, 102, 241, 0.2)',
        }
      },
      borderRadius: {
        'none': '0',
        'sm': '4px',
        DEFAULT: '8px',
        'md': '12px',
        'lg': '16px',
        'xl': '20px',
        '2xl': '24px',
      },
      backdropBlur: {
        xs: '2px',
        sm: '4px',
        md: '8px',
        lg: '16px',
        xl: '24px',
      },
      boxShadow: {
        'glass': '0 8px 32px rgba(0, 0, 0, 0.3)',
        'glass-sm': '0 4px 16px rgba(0, 0, 0, 0.2)',
        'glass-lg': '0 16px 48px rgba(0, 0, 0, 0.4)',
        'glow': '0 0 20px rgba(0, 217, 255, 0.3)',
        'glow-sm': '0 0 10px rgba(0, 217, 255, 0.2)',
        'glow-purple': '0 0 20px rgba(168, 85, 247, 0.3)',
      },
      animation: {
        'pulse-glow': 'pulse-glow 2s ease-in-out infinite',
        'gradient': 'gradient 8s ease infinite',
      },
      keyframes: {
        'pulse-glow': {
          '0%, 100%': { opacity: '1' },
          '50%': { opacity: '0.5' },
        },
        'gradient': {
          '0%, 100%': { backgroundPosition: '0% 50%' },
          '50%': { backgroundPosition: '100% 50%' },
        },
      },
    },
  },
  plugins: [],
}
