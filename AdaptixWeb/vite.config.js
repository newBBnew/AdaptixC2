import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import tailwindcss from '@tailwindcss/vite'
import fs from 'fs'
import path from 'path'

// Load the server certificate from the release directory
// This allows the proxy to verify the self-signed certificate of the Teamserver,
// similar to how the Qt client uses the cert file.
const certPath = path.resolve(__dirname, '../release/server.rsa.crt');
let serverCert = null;
try {
  serverCert = fs.readFileSync(certPath);
  console.log('Loaded server certificate from:', certPath);
} catch (e) {
  console.warn('Could not load server certificate, falling back to insecure proxy:', e.message);
}

// https://vite.dev/config/
export default defineConfig({
  plugins: [
    react(),
    tailwindcss(),
  ],
  build: {
    minify: 'terser',
    terserOptions: {
      mangle: false,  // Disable variable name mangling to prevent TDZ issues
      compress: {
        keep_fnames: true,
        keep_classnames: true,
      },
    },
  },
  server: {
    proxy: {
      '/api': {
        target: 'https://127.0.0.1:4321',
        changeOrigin: true,
        secure: !!serverCert,
        ca: serverCert,
        ws: true,
        rewrite: (path) => path.replace(/^\/api/, ''),
      },
    },
  },
})
