/**
 * AxScript - Web client implementation of AdaptixC2 extension scripting
 * Provides ax.* API compatible with Qt client's BridgeApp
 */

import { scriptApi } from '../api/control';

class AxScriptEngine {
  constructor() {
    this.commands = new Map();
    this.agentCommands = new Map(); // Per-agent command definitions: agentName -> Map(cmdName -> AxCommand)
    this.plugins = [];
    this.loadedScripts = new Set();
    this.loadedAgentScripts = new Set();
    this.basePath = '';
    this.agents = {};
    this.onCommandsUpdated = null;
    this.onExecuteCommand = null;
    this.onConsoleMessage = null;
  }

  setAgents(agents) {
    this.agents = agents;
  }

  setOnCommandsUpdated(callback) {
    this.onCommandsUpdated = callback;
  }

  setOnExecuteCommand(callback) {
    this.onExecuteCommand = callback;
  }

  setOnConsoleMessage(callback) {
    this.onConsoleMessage = callback;
  }

  // Execute agent-specific ax_config.axs script and register commands
  async loadAgentScript(agentName, axScript, listeners) {
    if (this.loadedAgentScripts.has(agentName)) {
      return;
    }
    
    try {
      const agentCmds = new Map();
      const agentAxAPI = this.createAxAPI('');
      
      agentAxAPI.create_command = (name, description, example, message) => {
        const command = new AxCommand(name, description, example, message);
        agentCmds.set(name, command);
        this.commands.set(name, command);
        const varName = `cmd_${name.replace(/[\s-]+/g, '_').toLowerCase()}`;
        window[varName] = command;
        return command;
      };

      window.ax = agentAxAPI;
      window.menu = this.createMenuAPI();
      window.form = this.createFormAPI();
      window.event = this.createEventAPI();

      const wrappedScript = `
        (function() {
          var ax = window.ax;
          var menu = window.menu;
          var form = window.form;
          var event = window.event;
          try {
            ${axScript}
          } catch (e) {
            console.error('[AxScript] Error in agent script ${agentName}:', e);
          }
        })();
      `;
      
      window.eval(wrappedScript);

      if (typeof window.RegisterCommands === 'function') {
        for (const listenerType of listeners) {
          try {
            const result = window.RegisterCommands(listenerType);
            if (result && result.commands_windows) {
              const cmdsArray = Array.isArray(result.commands_windows) 
                ? result.commands_windows 
                : [result.commands_windows];
              
              cmdsArray.forEach(cmd => {
                if (cmd instanceof AxCommand || (cmd && cmd.name)) {
                  agentCmds.set(cmd.name, cmd);
                  this.commands.set(cmd.name, cmd);
                  const varName = `cmd_${cmd.name.replace(/[\s-]+/g, '_').toLowerCase()}`;
                  window[varName] = cmd;
                }
              });
            }
          } catch (err) {
            console.error(`[AxScript] RegisterCommands(${listenerType}) failed:`, err);
          }
        }
      }

      this.agentCommands.set(agentName, agentCmds);
      this.loadedAgentScripts.add(agentName);
      this.onCommandsUpdated?.();
      
    } catch (err) {
      console.error(`[AxScript] Failed to load agent script ${agentName}:`, err);
    }
  }

  getAgentCommand(agentName, commandName) {
    const agentCmds = this.agentCommands.get(agentName);
    return agentCmds ? agentCmds.get(commandName) : null;
  }

  getAllCommands(agentName) {
    const combined = new Map(this.commands);
    const agentCmds = this.agentCommands.get(agentName);
    if (agentCmds) {
      agentCmds.forEach((cmd, name) => combined.set(name, cmd));
    }
    return combined;
  }

  async init() {
    try {
      const res = await scriptApi.getBasePath();
      if (res.data?.ok) {
        this.basePath = res.data.path;
        return true;
      }
      return false;
    } catch (err) {
      return false;
    }
  }

  async loadMainScript() {
    if (!this.basePath) return false;
    try {
      await this.loadScript('extension-kit.axs');
      this.onCommandsUpdated?.();
      return true;
    } catch (err) {
      return false;
    }
  }

  async loadScript(relativePath, scriptDir = '') {
    let normalizedPath = relativePath.replace(/\\/g, '/');
    if ((normalizedPath.startsWith('./') || normalizedPath.startsWith('../')) && scriptDir) {
      const fullPath = scriptDir + normalizedPath;
      const segments = fullPath.split('/');
      const resolvedSegments = [];
      for (const segment of segments) {
        if (segment === '..') {
          if (resolvedSegments.length > 0) resolvedSegments.pop();
        } else if (segment !== '.' && segment !== '') {
          resolvedSegments.push(segment);
        }
      }
      normalizedPath = resolvedSegments.join('/');
    }

    normalizedPath = normalizedPath.replace(/^(\.\.?\/)+/, '');
    if (normalizedPath.includes('Extension-Kit/')) {
      const index = normalizedPath.lastIndexOf('Extension-Kit/');
      normalizedPath = normalizedPath.substring(index + 'Extension-Kit/'.length);
    }
    if (normalizedPath.startsWith('/')) normalizedPath = normalizedPath.substring(1);

    if (this.loadedScripts.has(normalizedPath)) return;
    this.loadedScripts.add(normalizedPath);

    let currentScriptDir = '';
    const pathParts = normalizedPath.split('/');
    if (pathParts.length > 1) {
      pathParts.pop();
      currentScriptDir = pathParts.join('/') + '/';
    }

    try {
      const res = await scriptApi.read(normalizedPath);
      if (!res.data?.ok) throw new Error(res.data?.message || 'Read failed');
      await this.executeScript(res.data.content, normalizedPath, currentScriptDir);
    } catch (err) {
      console.error(`[AxScript] Error loading ${normalizedPath}:`, err);
      throw err;
    }
  }

  async executeScript(content, filename, scriptDir) {
    try {
      const preparedContent = content
        .replace(/^var\s+(cmd_\w+)\s*=/gm, '$1 =')
        .replace(/^var\s+(menu_\w+)\s*=/gm, '$1 =');

      const cmdRefs = content.match(/\bcmd_[a-zA-Z0-9_]+\b/g) || [];
      const menuRefs = content.match(/\bmenu_[a-zA-Z0-9_]+\b/g) || [];
      const allRefs = [...new Set([...cmdRefs, ...menuRefs])];
      for (const varName of allRefs) {
        if (typeof window[varName] === 'undefined') window[varName] = null;
      }

      if (filename === 'extension-kit.axs') window.axEngine = this;

      window.ax = this.createAxAPI(scriptDir);
      window.menu = this.createMenuAPI();
      window.form = this.createFormAPI();
      window.event = this.createEventAPI();

      const finalCode = `
        (function() {
          var ax = window.ax;
          var menu = window.menu;
          var form = window.form;
          var event = window.event;
          try {
            ${preparedContent}
          } catch (e) {
            console.error('[AxScript] Runtime error in ${filename}:', e);
          }
        })();
      `;
      window.eval(finalCode);
    } catch (err) {
      console.error(`[AxScript] Compilation error in ${filename}:`, err);
      throw err;
    }
  }

  createAxAPI(currentDir) {
    const engine = this;
    return {
      script_dir: () => currentDir,
      script_load: async (path) => {
        let rp = path.replace(/\\/g, '/');
        if (rp.includes('Extension-Kit/')) rp = rp.substring(rp.lastIndexOf('Extension-Kit/') + 14);
        if ((rp.startsWith('./') || rp.startsWith('../')) && currentDir) {
          const stack = (currentDir + rp).split('/');
          const res = [];
          for (const p of stack) {
            if (p === '..') { if (res.length) res.pop(); }
            else if (p !== '.' && p !== '') res.push(p);
          }
          rp = res.join('/');
        } else if (!rp.startsWith('/') && currentDir && !rp.includes('/')) {
          rp = currentDir + rp;
        }
        rp = rp.replace(/^(\.\.?\/)+/, '').replace(/^\//, '');
        if (!engine.loadedScripts.has(rp)) await engine.loadScript(rp, currentDir);
      },
      script_import: async (path) => {
        let rp = path.replace(/\\/g, '/');
        if (rp.includes('Extension-Kit/')) rp = rp.substring(rp.lastIndexOf('Extension-Kit/') + 14);
        if ((rp.startsWith('./') || rp.startsWith('../')) && currentDir) {
          const stack = (currentDir + rp).split('/');
          const res = [];
          for (const p of stack) {
            if (p === '..') { if (res.length) res.pop(); }
            else if (p !== '.' && p !== '') res.push(p);
          }
          rp = res.join('/');
        }
        rp = rp.replace(/^(\.\.?\/)+/, '').replace(/^\//, '');
        if (!engine.loadedScripts.has(rp)) await engine.loadScript(rp, currentDir);
      },
      create_command: (name, description, example) => {
        const cmd = new AxCommand(name, description, example);
        engine.commands.set(name, cmd);
        window[`cmd_${name.replace(/[\s-]+/g, '_').toLowerCase()}`] = cmd;
        return cmd;
      },
      create_commands_group: (name, commands) => Array.isArray(commands) ? commands : [commands],
      register_commands_group: (group) => {
        if (Array.isArray(group)) {
          group.forEach(cmd => {
            if (cmd instanceof AxCommand || (cmd && cmd.name)) {
              engine.commands.set(cmd.name, cmd);
              window[`cmd_${cmd.name.replace(/[\s-]+/g, '_').toLowerCase()}`] = cmd;
            }
          });
        }
      },
      plugin_register: (category, command, agents, os) => { engine.plugins.push({ category, command, agents, os }); },
      plugin_list: () => engine.plugins,
      agents: () => engine.agents,
      agent_info: (id, prop) => engine.agents[id] ? engine.agents[id][prop] : null,
      arch: (id) => engine.agents[id]?.a_arch || 'x64',
      is64: (id) => engine.agents[id]?.a_arch === 'x64',
      isadmin: (id) => !!engine.agents[id]?.a_elevated,
      file_basename: (p) => p.split(/[/\\]/).pop(),
      file_exists: async (p) => (await scriptApi.read(p)).data?.ok,
      file_read: async (p) => (await scriptApi.read(p)).data?.content || "",
      format_time: (t) => new Date(t * 1000).toLocaleString(),
      ticks: () => Math.floor(Date.now() / 1000),
      random_string: (l) => Math.random().toString(36).substring(2, 2 + l),
      show_message: (m) => alert(m),
      prompt_open_file: (t) => prompt("Open file: " + t),
      bof_pack: (t, a) => btoa(JSON.stringify({ t, a })),
      execute_alias: (id, cl, c, m, h) => engine.onExecuteCommand?.(id, cl, c, m, h),
      execute_alias_handler: (id, cl, c, m, h) => engine.onExecuteCommand?.(id, cl, c, m, h),
      execute_command: (id, cl, h, hd) => engine.onExecuteCommand?.(id, cl, h, hd),
      console_message: (id, m, t, tx) => engine.onConsoleMessage?.(id, m, t, tx),
      log: (m) => console.log(`[AxScript Log] ${m}`),
      log_error: (m) => console.error(`[AxScript Error] ${m}`),
      targets: () => [],
      credentials_add: () => {},
      credentials_add_list: () => {},
      targets_add_list: () => {},
    };
  }

  createMenuAPI() {
    const engine = this;
    const createMenuWrapper = (label) => {
      const m = {
        label, items: [],
        addItem: function(i) { this.items.push(i); return this; },
        addAction: function(a) { this.items.push(a); return this; },
        addSeparator: function() { this.items.push({ type: 'separator' }); return this; },
        addMenu: function(sm) { this.items.push(sm); return this; }
      };
      m.addItem = m.addItem.bind(m); m.addAction = m.addAction.bind(m);
      m.addSeparator = m.addSeparator.bind(m); m.addMenu = m.addMenu.bind(m);
      return m;
    };
    return {
      create_action: (label, callback) => ({ label, callback, type: 'action' }),
      create_menu: (label) => createMenuWrapper(label),
      create_separator: () => ({ type: 'separator' }),
      add_session_browser: (a, ag, o) => engine.plugins.push({ category: 'Browser', command: a.label || a, agents: ag, os: o }),
      add_session_access: (a, ag, o) => engine.plugins.push({ category: 'Access', command: a.label || a, agents: ag, os: o }),
      add_targets: (a, ag, o) => engine.plugins.push({ category: 'Targets', command: a.label || a, agents: ag, os: o }),
      add_session_agent: (a, ag, o) => engine.plugins.push({ category: (typeof a === 'string' ? a : (a.label || 'Agent')), command: a.label || a, agents: ag, os: o }),
      add_downloads_running: (a, ag, o) => engine.plugins.push({ category: 'Downloads', command: a.label || a, agents: ag, os: o }),
      add_downloads_stopped: (a, ag, o) => engine.plugins.push({ category: 'Downloads', command: a.label || a, agents: ag, os: o }),
      add_downloads_finished: (a, ag, o) => engine.plugins.push({ category: 'Downloads', command: a.label || a, agents: ag, os: o }),
      add_tasks_job: (a, ag, o) => engine.plugins.push({ category: 'Jobs', command: a.label || a, agents: ag, os: o }),
      add_processbrowser: (a, ag, o) => engine.plugins.push({ category: 'ProcessBrowser', command: a.label || a, agents: ag, os: o }),
      add_filebrowser: (a, ag, o) => engine.plugins.push({ category: 'FileBrowser', command: a.label || a, agents: ag, os: o })
    };
  }

  createFormAPI() {
    return {
      create: (title) => ({
        title, fields: [],
        addLabel: function(t) { this.fields.push({ type: 'label', text: t }); return this; },
        addTextField: function(n, l, d) { this.fields.push({ type: 'text', name: n, label: l, defaultValue: d }); return this; },
        addTextArea: function(n, l, d) { this.fields.push({ type: 'textarea', name: n, label: l, defaultValue: d }); return this; },
        addComboBox: function(n, l, o, di) { this.fields.push({ type: 'combo', name: n, label: l, options: o, defaultIndex: di }); return this; },
        addCheckBox: function(n, l, d) { this.fields.push({ type: 'checkbox', name: n, label: l, defaultValue: d }); return this; },
        addFileField: function(n, l) { this.fields.push({ type: 'file', name: n, label: l }); return this; },
        show: function(cb) { cb?.({}); }
      })
    };
  }

  createEventAPI() {
    return {
      on: () => {}, emit: () => {},
      on_filebrowser_list: () => {}, on_filebrowser_disks: () => {}, on_filebrowser_upload: () => {}, on_processbrowser_list: () => {}
    };
  }

  getCommands() { return Array.from(this.commands.values()); }
  getCommand(name) { return this.commands.get(name.split(' ')[0]); }
}

class AxCommand {
  constructor(name, description, example, message) {
    this.name = name; this.description = description; this.example = example;
    this.message = message || ""; this.args = []; this.pre_hook = null;
    this.post_hook = null; this.handler = null; this.subcommands = []; this.is_pre_hook = false;
  }
  addArgString(n, r, d) { this.args.push({ name: n, type: 'STRING', required: r, description: d, mark: '', flag: false }); return this; }
  addArgInt(n, r, d) { this.args.push({ name: n, type: 'INT', required: r, description: d, mark: '', flag: false }); return this; }
  addArgFile(n, r, d) { this.args.push({ name: n, type: 'FILE', required: r, description: d, mark: '', flag: false }); return this; }
  addArgFlagString(m, n, d, dv) { this.args.push({ name: n, type: 'STRING', required: false, description: d, mark: m, defaultValue: dv, flag: true }); return this; }
  addArgFlagInt(m, n, d, dv) { this.args.push({ name: n, type: 'INT', required: false, description: d, mark: m, defaultValue: dv, flag: true }); return this; }
  addArgFlagFile(m, n, d, dv) { this.args.push({ name: n, type: 'FILE', required: false, description: d, mark: m, defaultValue: dv, flag: true }); return this; }
  addArgBool(m, d) { this.args.push({ name: m, type: 'BOOL', required: false, description: d, mark: m, flag: true }); return this; }
  setPreHook(fn) { this.pre_hook = fn; this.is_pre_hook = (typeof fn === 'function'); return this; }
  addSubcommand(n, d, e) { const s = new AxCommand(n, d, e); this.subcommands.push(s); return s; }
  addSubCommands(a) {
    if (Array.isArray(a)) a.forEach(s => { if (s instanceof AxCommand) this.subcommands.push(s); });
    else if (a instanceof AxCommand) this.subcommands.push(a);
    return this;
  }
}

export const axEngine = new AxScriptEngine();
export default axEngine;
