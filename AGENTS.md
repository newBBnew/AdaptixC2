# adaptix Development Guide

This file provides guidelines for AI agents working on the adaptix codebase.

## Project Overview

adaptix is a red team C2 (Command & Control) framework with:
- **AdaptixWeb**: React 19 frontend (Vite + TailwindCSS 4)
- **AdaptixServer**: Go-based C2 server
- **AdaptixClient**: C++ implant
- **adaptix_mcp**: MCP server for Go-based integration

## mgrep Semantic Search Integration

This project uses **mgrep** for semantic code search. mgrep provides AI-powered search across code, images, PDFs and more.

### Tool Definition (mgrep.ts)

mgrep integrates with OpenCode through a tool definition at `~/.config/opencode/tool/mgrep.ts`:

**Arguments:**
| Arg | Type | Default | Description |
|-----|------|---------|-------------|
| `q` | `string` | (required) | The semantic search query |
| `m` | `number` | `10` | Number of chunks to return |
| `a` | `boolean` | `false` | Whether to generate an answer based on chunks |

**Usage Pattern:**
```
Search for "authentication logic" in the codebase
mgrep q="how authentication is handled" m=5
mgrep q="React context patterns" m=10 a=true
```

### MCP Server Configuration

Background file synchronization is enabled via MCP server in `opencode.json`:
```json
{
  "mcp": {
    "mgrep": {
      "type": "local",
      "command": ["mgrep", "mcp"],
      "enabled": true
    }
  }
}
```

### Installation

```bash
# Install mgrep tool definition and MCP config for OpenCode
mgrep install-opencode

# Uninstall
mgrep uninstall-opencode
```

### Key Benefits

- **Semantic Search**: Understands code intent, not just text matching
- **Background Sync**: MCP server keeps file index updated automatically
- **File Navigation**: Returns file paths with line ranges for direct navigation

## Build Commands

### AdaptixWeb (Frontend)

```bash
cd AdaptixWeb

# Development
npm run dev

# Production build
npm run build

# Lint (ESLint)
npm run lint

# Preview production build
npm run preview
```

### Docker (Full Stack)

```bash
# Build server only
docker compose --profile build-server up --build

# Build extenders only
docker compose --profile build-extenders up --build

# Build server + extenders
docker compose --profile build-server-ext up --build

# Run server runtime
docker compose --profile runtime up --build
```

## Code Style Guidelines

### Imports and Organization

- Use named imports: `import { something } from 'module'`
- Default imports for React: `import React from 'react'`
- Group imports: React → external libraries → internal components/utilities
- Use the `cn()` utility from `src/utils/cn.js` for className composition:

```jsx
import { cn } from '../utils/cn';

function Component({ className }) {
  return <div className={cn("base-class", className)} />;
}
```

### Component Patterns

- Use functional components with hooks
- Use Context API for shared state (Pattern: `XxxContext.jsx` + `useXxx()` hook)
- Component files: PascalCase (e.g., `Dashboard.jsx`)
- Context files: PascalCase ending in `Context` (e.g., `AgentContext.jsx`)
- Helper files: camelCase (e.g., `cn.js`, `configUtils.js`)

### Hook Naming

- Custom hooks: `useXxx` pattern (camelCase, starts with "use")
- Use `useCallback`, `useMemo`, `useRef` for optimization
- Use lazy state initialization when accessing localStorage:

```jsx
const [state, setState] = useState(() => {
  try {
    return localStorage.getItem('key') || defaultValue;
  } catch (e) {
    return defaultValue;
  }
});
```

### Helper Functions (Private)

- Prefix private helper functions with underscore: `_helperName()`
- Keep complex logic in standalone helper functions
- Document edge cases in code logic (not in comments)

### Naming Conventions

- Variables/functions: `camelCase`
- Components/contexts: `PascalCase`
- Constants: `UPPER_SNAKE_CASE` or `kebab-case` for CSS classes
- File names: React components use `PascalCase.jsx`, utilities use `camelCase.js`
- State variables: `camelCase` with descriptive names

### Error Handling

- Use try/catch for async operations
- Return error objects with `{ ok: false, error: ... }` pattern
- Log errors with contextual prefix: `console.error('[ContextName] Error:', err)`
- Handle localStorage errors gracefully

### State Updates

- Use functional state updates: `setState(prev => ...)`
- Use spread operator for immutable updates: `setX(prev => ({ ...prev, key: value }))`
- Batch state updates when possible
- Use Refs for mutable values that don't trigger re-renders

### API Patterns

- Axios instance with interceptors for auth tokens
- API modules export an object with methods:

```javascript
export const apiModule = {
  list: () => api.get('/path'),
  create: (data) => api.post('/path', data),
  update: (id, data) => api.post(`/path/${id}`, data),
  remove: (ids) => api.post('/path/remove', { id_array: ids }),
};
```

### WebSocket/Real-time

- Use Context to manage WebSocket connections
- Packet handling: switch statement on `packet.type`
- Use packet type constants from `src/constants/packetTypes.js`

### Styling (TailwindCSS 4)

- Use utility classes directly in JSX
- Use `cn()` utility for conditional classes
- No custom CSS classes unless necessary
- Tailwind v4 uses `@tailwindcss/postcss`

### ESLint Rules

- `no-unused-vars`: Error, except variables matching `^[A-Z_]` (constants)
- Use Flat Config format (ESLint 9+)
- No comments in code unless explicitly requested

### File Structure

```
AdaptixWeb/src/
├── api/           # API modules (agent.js, control.js)
├── components/    # Reusable UI components
├── context/       # React contexts + hooks
├── pages/         # Route components
├── utils/         # Helper utilities
├── constants/     # Constants
└── assets/        # Static assets
```

## Testing

- No test framework currently configured
- Manual testing via `npm run dev`
- Production build verification: `npm run build && npm run preview`

## Key Files

- `vite.config.js`: Vite config with proxy to `https://127.0.0.1:4321`
- `eslint.config.js`: ESLint Flat Config
- `postcss.config.js`: PostCSS with TailwindCSS 4
- `src/api/agent.js`: Agent control API with auth interceptor
- `src/context/AgentContext.jsx`: Agent state management
- `src/utils/cn.js`: Tailwind className composer

## Notes

- Frontend proxies API calls to `https://127.0.0.1:4321` (C2 server)
- Auth token stored in `localStorage.getItem('adaptix_token')`
- React Router v7 with basename `/ui`
- xterm.js for terminal components
- Framer Motion for animations
