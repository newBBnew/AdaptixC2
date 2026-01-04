import { AxCommand } from './axCommand';

/**
 * AxScript - Web client implementation of AdaptixC2 extension scripting
 * Provides ax.* API compatible with Qt client's BridgeApp
 */

export const createAxEngine = () => {
  const engine = {
    commands: new Map(),
    commandGroups: [], // Track command groups for help display
    agentCommands: new Map(),
    plugins: [],
    loadedScripts: new Set(),
    loadedAgentScripts: new Set(),
    loadingScripts: new Set(),
    basePath: '',
    agents: {},
    onCommandsUpdated: null,
    onExecuteCommand: null,
    onConsoleMessage: null,
    _scriptApi: null,

    setScriptApi(api) {
      this._scriptApi = api;
    },

    get scriptApi() {
      return this._scriptApi;
    },

    setAgents(agents) {
      this.agents = agents;
    },

    setOnCommandsUpdated(callback) {
      this.onCommandsUpdated = callback;
    },

    setOnExecuteCommand(callback) {
      this.onExecuteCommand = callback;
    },

    setOnConsoleMessage(callback) {
      this.onConsoleMessage = callback;
    },

    async loadAgentScript(agentName, axScript, listeners) {
      if (this.loadedAgentScripts.has(agentName)) return;
      try {
        console.log(`[AxScript] Loading agent script for ${agentName}...`);
        const agentCmds = new Map();
        
        const self = this;
        const agentAxAPI = self.createAxAPI('');
        
        agentAxAPI.create_command = (name, description, example, message) => {
          const command = new AxCommand(name, description, example, message);
          agentCmds.set(name, command);
          if (!self.commands.has(name)) {
            self.commands.set(name, command);
          }
          return command;
        };
        
        const scriptContext = {
          ax: agentAxAPI,
          menu: self.createMenuAPI(),
          form: self.createFormAPI(),
          event: self.createEventAPI()
        };

        const agentScope = {};
        // REFACTOR: Deep isolate script execution context
        // We explicitly shield common minified global names (Ot, Wt, etc.) if they leak,
        // and force the script to only see the provided APIs.
        const scriptFunc = new Function('ax', 'menu', 'form', 'event', 'scope', 'window', 'globalThis', 'self', `
          "use strict";
          var RegisterCommands = undefined;
          try {
            ${axScript}
            if (typeof RegisterCommands === 'function') {
              scope.RegisterCommands = RegisterCommands;
            }
          } catch (e) {
            console.error('[AxScript] Error in agent script execution:', e);
          }
        `);

        // Pass empty objects for global-like objects to prevent the script from accessing minified symbols
        const proxyEnv = {};
        scriptFunc(scriptContext.ax, scriptContext.menu, scriptContext.form, scriptContext.event, agentScope, proxyEnv, proxyEnv, proxyEnv);

        if (typeof agentScope.RegisterCommands === 'function') {
          for (const listenerType of listeners) {
            try {
              const result = agentScope.RegisterCommands(listenerType);
              if (result && result.commands_windows) {
                const cmdsArray = Array.isArray(result.commands_windows) ? result.commands_windows : [result.commands_windows];
                cmdsArray.forEach(cmd => {
                  if (cmd instanceof AxCommand || (cmd && cmd.name)) {
                    agentCmds.set(cmd.name, cmd);
                    if (!self.commands.has(cmd.name)) {
                      self.commands.set(cmd.name, cmd);
                    }
                  }
                });
              }
            } catch (err) { console.error(`[AxScript] RegisterCommands(${listenerType}) failed:`, err); }
          }
        }
        self.agentCommands.set(agentName, agentCmds);
        self.loadedAgentScripts.add(agentName);
        self.onCommandsUpdated?.();
      } catch (err) { console.error(`[AxScript] Failed to load agent script ${agentName}:`, err); }
    },

    getAgentCommand(agentName, commandName) {
      const agentCmds = this.agentCommands.get(agentName);
      return agentCmds ? agentCmds.get(commandName) : null;
    },

    getAllCommands(agentName) {
      const combined = new Map(this.commands);
      const agentCmds = this.agentCommands.get(agentName);
      if (agentCmds) agentCmds.forEach((cmd, name) => combined.set(name, cmd));
      return combined;
    },

    // Get command list for autocomplete (matches Qt client GetCommands)
    getCommandList(agentName) {
      const commandList = [];
      const helpList = [];
      const allCmds = this.getAllCommands(agentName);
      
      allCmds.forEach((cmd, name) => {
        helpList.push(`help ${name}`);
        if (!cmd.subcommands || cmd.subcommands.length === 0) {
          commandList.push(name);
        } else {
          cmd.subcommands.forEach(sub => {
            commandList.push(`${name} ${sub.name}`);
            helpList.push(`help ${name} ${sub.name}`);
          });
        }
      });
      
      return [...commandList, ...helpList, 'reload'];
    },

    // Get command groups for help display
    getCommandGroups() {
      return this.commandGroups;
    },

    async init() {
      if (!this._scriptApi) return false;
      try {
        const res = await this._scriptApi.getBasePath();
        if (res.data?.ok) {
          this.basePath = res.data.path;
          return true;
        }
        return false;
      } catch (err) { return false; }
    },

    async loadMainScript() {
      if (!this.basePath) return false;
      try {
        await this.loadScript('extension-kit.axs');
        this.onCommandsUpdated?.();
        return true;
      } catch (err) { return false; }
    },

    async loadScript(relativePath, scriptDir = '') {
      let normalizedPath = relativePath.replace(/\\/g, '/');
      if ((normalizedPath.startsWith('./') || normalizedPath.startsWith('../')) && scriptDir) {
        const fullPath = scriptDir + normalizedPath;
        const segments = fullPath.split('/');
        const resolvedSegments = [];
        for (const segment of segments) {
          if (segment === '..') { if (resolvedSegments.length > 0) resolvedSegments.pop(); }
          else if (segment !== '.' && segment !== '') resolvedSegments.push(segment);
        }
        normalizedPath = resolvedSegments.join('/');
      }
      normalizedPath = normalizedPath.replace(/^(\.\.?\/)+/, '');
      if (normalizedPath.includes('Extension-Kit/')) {
        const index = normalizedPath.lastIndexOf('Extension-Kit/');
        normalizedPath = normalizedPath.substring(index + 'Extension-Kit/'.length);
      }
      if (normalizedPath.startsWith('/')) normalizedPath = normalizedPath.substring(1);
      
      // Use a lock to prevent concurrent loading of the same script
      if (this.loadingScripts?.has(normalizedPath)) return;
      if (!this.loadingScripts) this.loadingScripts = new Set();
      if (this.loadedScripts.has(normalizedPath)) return;
      
      this.loadingScripts.add(normalizedPath);
      
      let currentScriptDir = '';
      const pathParts = normalizedPath.split('/');
      if (pathParts.length > 1) { pathParts.pop(); currentScriptDir = pathParts.join('/') + '/'; }
      try {
        if (!this._scriptApi) throw new Error('scriptApi not injected');
        const res = await this._scriptApi.read(normalizedPath);
        if (!res.data?.ok) throw new Error(res.data?.message || 'Read failed');
        
        // Double check after async read
        if (this.loadedScripts.has(normalizedPath)) return;
        
        await this.executeScript(res.data.content, normalizedPath, currentScriptDir);
        this.loadedScripts.add(normalizedPath);
      } catch (err) { 
        console.error(`[AxScript] Error loading ${normalizedPath}:`, err); 
        throw err; 
      } finally {
        this.loadingScripts.delete(normalizedPath);
      }
    },

    async executeScript(content, filename, scriptDir) {
      try {
        const preparedContent = content.replace(/^var\s+(cmd_\w+)\s*=/gm, '$1 =').replace(/^var\s+(menu_\w+)\s*=/gm, '$1 =');
        const cmdRefs = content.match(/\bcmd_[a-zA-Z0-9_]+\b/g) || [];
        const menuRefs = content.match(/\bmenu_[a-zA-Z0-9_]+\b/g) || [];
        const allRefs = [...new Set([...cmdRefs, ...menuRefs])];
        for (const varName of allRefs) { if (typeof window[varName] === 'undefined') window[varName] = null; }

        if (filename === 'extension-kit.axs') window.axEngine = this;

        // Collect pending script loads to await after execution
        const pendingLoads = [];
        const scriptContext = {
          ax: this.createAxAPI(scriptDir, pendingLoads),
          menu: this.createMenuAPI(),
          form: this.createFormAPI(),
          event: this.createEventAPI()
        };

        // REFACTOR: Deep isolate script execution context
        const scriptFunc = new Function('ax', 'menu', 'form', 'event', 'window', 'globalThis', 'self', `
          "use strict";
          try {
            ${preparedContent}
          } catch (e) {
            console.error('[AxScript] Runtime error in ${filename}:', e);
            throw e;
          }
        `);

        // Pass empty objects for global-like objects to prevent the script from accessing minified symbols
        const proxyEnv = {};
        scriptFunc(scriptContext.ax, scriptContext.menu, scriptContext.form, scriptContext.event, proxyEnv, proxyEnv, proxyEnv);
        
        // Wait for all script_load calls to complete
        if (pendingLoads.length > 0) {
          console.log(`[AxScript] Waiting for ${pendingLoads.length} sub-scripts to load...`);
          await Promise.all(pendingLoads);
          console.log(`[AxScript] All sub-scripts loaded, total commands: ${this.commands.size}`);
        }
      } catch (err) { 
        console.error(`[AxScript] Execution error in ${filename}:`, err); 
        throw err; 
      }
    },

    createAxAPI(currentDir, pendingLoads = null) {
      const self = this;
      return {
        script_dir: () => currentDir,
        script_load: (path) => {
          let rp = path.replace(/\\/g, '/');
          if (rp.includes('Extension-Kit/')) rp = rp.substring(rp.lastIndexOf('Extension-Kit/') + 14);
          if ((rp.startsWith('./') || rp.startsWith('../')) && currentDir) {
            const stack = (currentDir + rp).split('/');
            const res = [];
            for (const p of stack) { if (p === '..') { if (res.length) res.pop(); } else if (p !== '.' && p !== '') res.push(p); }
            rp = res.join('/');
          } else if (!rp.startsWith('/') && currentDir && !rp.includes('/')) { rp = currentDir + rp; }
          rp = rp.replace(/^(\.\.?\/)+/, '').replace(/^\//, '');
          if (!self.loadedScripts.has(rp)) {
            const loadPromise = self.loadScript(rp, currentDir);
            if (pendingLoads) pendingLoads.push(loadPromise);
            return loadPromise;
          }
        },
        script_load_agent: async (agentName, axScript, listeners) => {
          await self.loadAgentScript(agentName, axScript, listeners);
        },
        script_import: async (path) => {
          let rp = path.replace(/\\/g, '/');
          if (rp.includes('Extension-Kit/')) rp = rp.substring(rp.lastIndexOf('Extension-Kit/') + 14);
          if ((rp.startsWith('./') || rp.startsWith('../')) && currentDir) {
            const stack = (currentDir + rp).split('/');
            const res = [];
            for (const p of stack) { if (p === '..') { if (res.length) res.pop(); } else if (p !== '.' && p !== '') res.push(p); }
            rp = res.join('/');
          }
          rp = rp.replace(/^(\.\.?\/)+/, '').replace(/^\//, '');
          if (!self.loadedScripts.has(rp)) await self.loadScript(rp, currentDir);
        },
        create_command: (name, description, example) => {
          // Match Qt client: create_command does NOT register to global commands
          // Only register_commands_group should register commands
          const cmd = new AxCommand(name, description, example);
          window[`cmd_${name.replace(/[\s-]+/g, '_').toLowerCase()}`] = cmd;
          return cmd;
        },
        create_commands_group: (name, commands) => ({ groupName: name, commands: Array.isArray(commands) ? commands : [commands] }),
        register_commands_group: (group) => {
          if (group && group.groupName && Array.isArray(group.commands)) {
            // Track group for help display
            self.commandGroups.push({ groupName: group.groupName, commands: group.commands });
            group.commands.forEach(cmd => {
              if (cmd instanceof AxCommand || (cmd && cmd.name)) {
                self.commands.set(cmd.name, cmd);
                window[`cmd_${cmd.name.replace(/[\s-]+/g, '_').toLowerCase()}`] = cmd;
              }
            });
          } else if (Array.isArray(group)) {
            // Legacy format support
            group.forEach(cmd => {
              if (cmd instanceof AxCommand || (cmd && cmd.name)) {
                self.commands.set(cmd.name, cmd);
                window[`cmd_${cmd.name.replace(/[\s-]+/g, '_').toLowerCase()}`] = cmd;
              }
            });
          }
        },
        plugin_register: (category, command, agents, os) => { self.plugins.push({ category, command, agents, os }); },
        plugin_list: () => self.plugins,
        agents: () => self.agents,
        agent_info: (id, prop) => self.agents[id] ? self.agents[id][prop] : null,
        arch: (id) => self.agents[id]?.a_arch || 'x64',
        is64: (id) => self.agents[id]?.a_arch === 'x64',
        isadmin: (id) => !!self.agents[id]?.a_elevated,
        file_basename: (p) => p.split(/[/\\]/).pop(),
        file_exists: async (p) => self._scriptApi ? (await self._scriptApi.read(p)).data?.ok : false,
        file_read: async (p) => self._scriptApi ? (await self._scriptApi.read(p)).data?.content || "" : "",
        format_time: (t) => new Date(t * 1000).toLocaleString(),
        ticks: () => Math.floor(Date.now() / 1000),
        random_string: (l) => Math.random().toString(36).substring(2, 2 + l),
        show_message: (m) => alert(m),
        prompt_open_file: (t) => prompt("Open file: " + t),
        bof_pack: (types, args) => JSON.stringify({ __type: 'bof_descriptor', types, args }),
        // Notification Mode: Parse "execute bof ${bof_path} ${bof_params}" and send path + params to server
        execute_alias: (id, cmdline, aliasCmd, message, handler) => {
          // Parse aliasCmd to extract BOF path and params
          // Format: "execute bof /path/to/file.o [params_json]"
          const bofMatch = aliasCmd.match(/^execute\s+bof\s+([^\s]+)(?:\s+(.*))?$/i);
          if (bofMatch) {
            const bofPath = bofMatch[1];
            const paramData = bofMatch[2] || '';
            // Construct command data for notification mode
            const commandData = {
              command: 'execute',
              subcommand: 'bof',
              bof_path: bofPath,
              param_data: paramData,
              message: message || ''
            };
            self.onExecuteCommand?.(id, cmdline, commandData, message, handler);
          } else {
            // Non-BOF alias command: mark as direct execution
            self.onExecuteCommand?.(id, cmdline, { __direct_cmdline: aliasCmd, __original_cmdline: cmdline, message }, message, handler);
          }
        },
        execute_alias_handler: (id, cmdline, aliasCmd, message, handler) => {
          const bofMatch = aliasCmd.match(/^execute\s+bof\s+([^\s]+)(?:\s+(.*))?$/i);
          if (bofMatch) {
            const bofPath = bofMatch[1];
            const paramData = bofMatch[2] || '';
            const commandData = {
              command: 'execute',
              subcommand: 'bof',
              bof_path: bofPath,
              param_data: paramData,
              message: message || ''
            };
            self.onExecuteCommand?.(id, cmdline, commandData, message, handler);
          } else {
            self.onExecuteCommand?.(id, cmdline, { __direct_cmdline: aliasCmd, __original_cmdline: cmdline, message }, message, handler);
          }
        },
        execute_command: (id, cmdline, hookId, handlerId) => self.onExecuteCommand?.(id, cmdline, null, null, null),
        console_message: (id, m, t, tx) => self.onConsoleMessage?.(id, m, t, tx),
        log: (m) => console.log(`[AxScript Log] ${m}`),
        log_error: (m) => console.error(`[AxScript Error] ${m}`),
        targets: () => [],
        credentials_add: () => {},
        credentials_add_list: () => {},
        targets_add_list: () => {},
      };
    },

    createMenuAPI() {
      const self = this;
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
        add_session_browser: (a, ag, o) => self.plugins.push({ category: 'Browser', command: a.label || a, agents: ag, os: o }),
        add_session_access: (a, ag, o) => self.plugins.push({ category: 'Access', command: a.label || a, agents: ag, os: o }),
        add_targets: (a, ag, o) => self.plugins.push({ category: 'Targets', command: a.label || a, agents: ag, os: o }),
        add_session_agent: (a, ag, o) => self.plugins.push({ category: (typeof a === 'string' ? a : (a.label || 'Agent')), command: a.label || a, agents: ag, os: o }),
        add_downloads_running: (a, ag, o) => self.plugins.push({ category: 'Downloads', command: a.label || a, agents: ag, os: o }),
        add_downloads_stopped: (a, ag, o) => self.plugins.push({ category: 'Downloads', command: a.label || a, agents: ag, os: o }),
        add_downloads_finished: (a, ag, o) => self.plugins.push({ category: 'Downloads', command: a.label || a, agents: ag, os: o }),
        add_tasks_job: (a, ag, o) => self.plugins.push({ category: 'Jobs', command: a.label || a, agents: ag, os: o }),
        add_processbrowser: (a, ag, o) => self.plugins.push({ category: 'ProcessBrowser', command: a.label || a, agents: ag, os: o }),
        add_filebrowser: (a, ag, o) => self.plugins.push({ category: 'FileBrowser', command: a.label || a, agents: ag, os: o })
      };
    },

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
    },

    createEventAPI() {
      return {
        on: () => {}, emit: () => {},
        on_filebrowser_list: () => {}, on_filebrowser_disks: () => {}, on_filebrowser_upload: () => {}, on_processbrowser_list: () => {}
      };
    },

    getCommands() { return Array.from(this.commands.values()); },
    getCommand(name) { return this.commands.get(name.split(' ')[0]); }
  };
  return engine;
};

export default createAxEngine;
