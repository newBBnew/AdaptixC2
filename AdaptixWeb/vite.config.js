import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import tailwindcss from '@tailwindcss/vite'

// https://vite.dev/config/
export default defineConfig({
  plugins: [
    react(),
    tailwindcss(),
  ],
  server: {
    proxy: {
      '/login': {
        target: 'https://127.0.0.1:4321',
        changeOrigin: true,
        secure: false,
        rewrite: (path) => `/endpoint/login`
      },
      '/api': {
        target: 'https://127.0.0.1:4321',
        changeOrigin: true,
        secure: false,
        rewrite: (path) => path.replace(/^\/api/, '/endpoint')
      },
    },
  },
})
