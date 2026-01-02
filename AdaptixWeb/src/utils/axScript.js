/**
 * AxScript - Web client implementation of AdaptixC2 extension scripting
 * Provides ax.* API compatible with Qt client's BridgeApp
 */

import { scriptApi } from '../api/control';

class AxScriptEngine {
  constructor() {
    this.commands = new Map();
    this.plugins = [];
    this.loadedScripts = new Set();
    this.basePath = '';
    this.currentScriptDir = '';
    this.agents = {};
    this.onCommandsUpdated = null;
  }

  setAgents(agents) {
    this.agents = agents;
  }

  setOnCommandsUpdated(callback) {
    this.onCommandsUpdated = callback;
  }

  // Initialize and load the main extension script
  async init() {
    try {
      const res = await scriptApi.getBasePath();
      if (res.data?.ok) {
        this.basePath = res.data.path;
        console.log('[AxScript] Base path:', this.basePath);
        return true;
      }
      console.warn('[AxScript] Extension path not configured');
      return false;
    } catch (err) {
      console.error('[AxScript] Init failed:', err);
      return false;
    }
  }

  // Load the main extension-kit.axs entry point
  async loadMainScript() {
    if (!this.basePath) {
      console.warn('[AxScript] No base path configured');
      return false;
    }
    
    try {
      await this.loadScript('extension-kit.axs');
      console.log('[AxScript] Loaded extension-kit.axs');
      this.onCommandsUpdated?.();
      return true;
    } catch (err) {
      console.error('[AxScript] Failed to load main script:', err);
      return false;
    }
  }

  // Load a script file
  async loadScript(relativePath) {
    if (this.loadedScripts.has(relativePath)) {
      return;
    }
    this.loadedScripts.add(relativePath);

    const previousDir = this.currentScriptDir;
    const pathParts = relativePath.split('/');
    if (pathParts.length > 1) {
      pathParts.pop();
      this.currentScriptDir = pathParts.join('/') + '/';
    } else {
      this.currentScriptDir = '';
    }

    try {
      const res = await scriptApi.read(relativePath);
      if (!res.data?.ok) {
        throw new Error(res.data?.message || 'Failed to read script');
      }

      const content = res.data.content;
      await this.executeScript(content, relativePath);
    } finally {
      this.currentScriptDir = previousDir;
    }
  }

  // Execute script content
  async executeScript(content, filename) {
    // Create the ax API object
    const ax = this.createAxAPI();

    // Wrap in async function to allow await in script
    const wrappedCode = `
      (async function(ax, form, event, menu) {
        ${content}
      })
    `;

    try {
      const fn = eval(wrappedCode);
      await fn(ax, {}, {}, {});
    } catch (err) {
      console.error(`[AxScript] Error in ${filename}:`, err);
      throw err;
    }
  }

  // Create the ax.* API object
  createAxAPI() {
    const engine = this;

    return {
      // Script management
      script_dir: () => engine.currentScriptDir,
      script_load: async (path) => {
        // Normalize path - remove base path prefix if present
        let normalizedPath = path;
        if (path.startsWith(engine.basePath)) {
          normalizedPath = path.substring(engine.basePath.length);
        }
        // Handle relative paths like ax.script_dir() + "file.axs"
        if (!normalizedPath.startsWith('/')) {
          normalizedPath = engine.currentScriptDir + normalizedPath;
        }
        await engine.loadScript(normalizedPath.replace(/^\//, ''));
      },
      script_import: async (path) => {
        let normalizedPath = path;
        if (path.startsWith(engine.basePath)) {
          normalizedPath = path.substring(engine.basePath.length);
        }
        if (!normalizedPath.startsWith('/')) {
          normalizedPath = engine.currentScriptDir + normalizedPath;
        }
        await engine.loadScript(normalizedPath.replace(/^\//, ''));
      },

      // Command creation
      create_command: (name, description, example) => {
        const command = new AxCommand(name, description, example);
        engine.commands.set(name, command);
        return command;
      },

      // Plugin registration
      plugin_register: (category, command, agents, os) => {
        engine.plugins.push({ category, command, agents, os });
      },
      plugin_list: () => engine.plugins,

      // Agent info
      agents: () => engine.agents,
      agent_info: (id, property) => {
        const agent = engine.agents[id];
        return agent ? agent[property] : null;
      },
      arch: (id) => {
        const agent = engine.agents[id];
        return agent?.a_arch || 'x64';
      },

      // BOF packing
      bof_pack: (types, args) => {
        // Return a placeholder - actual packing done server-side
        return btoa(JSON.stringify({ types, args }));
      },

      // Command execution
      execute_alias: (id, cmdline, command, message, hook) => {
        console.log('[AxScript] execute_alias:', { id, cmdline, command, message });
        // This will be handled by the console component
        if (engine.onExecuteCommand) {
          engine.onExecuteCommand(id, cmdline, command, message, hook);
        }
      },

      // Console output
      console_message: (id, message, type) => {
        console.log(`[AxScript] [${type}] ${message}`);
      },
      log: (message) => {
        console.log('[AxScript]', message);
      },

      // Credentials
      credentials_add: (username, password, realm, type, tag, storage, host) => {
        console.log('[AxScript] credentials_add:', { username, realm, type, storage, host });
      },
    };
  }

  // Get all registered commands
  getCommands() {
    return Array.from(this.commands.values());
  }

  // Get command by name
  getCommand(name) {
    // Handle subcommands (e.g., "token make")
    const parts = name.split(' ');
    return this.commands.get(parts[0]);
  }
}

// Command definition class
class AxCommand {
  constructor(name, description, example) {
    this.name = name;
    this.description = description;
    this.example = example;
    this.args = [];
    this.preHook = null;
    this.subcommands = [];
  }

  addArgString(name, required, description) {
    this.args.push({ name, type: 'string', required, description, mark: '' });
    return this;
  }

  addArgFlagString(mark, name, description, defaultValue) {
    this.args.push({ name, type: 'string', required: false, description, mark, defaultValue });
    return this;
  }

  addArgFlagInt(mark, name, description, defaultValue) {
    this.args.push({ name, type: 'int', required: false, description, mark, defaultValue });
    return this;
  }

  addArgBool(mark, description) {
    this.args.push({ name: mark, type: 'bool', required: false, description, mark });
    return this;
  }

  setPreHook(fn) {
    this.preHook = fn;
    return this;
  }

  addSubcommand(name, description, example) {
    const subcmd = new AxCommand(name, description, example);
    this.subcommands.push(subcmd);
    return subcmd;
  }
}

// Singleton instance
export const axEngine = new AxScriptEngine();
export default axEngine;
