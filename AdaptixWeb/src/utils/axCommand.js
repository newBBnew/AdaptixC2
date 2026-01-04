export class AxCommand {
  constructor(name, description, example, message) {
    this.name = name;
    this.description = description;
    this.example = example;
    this.message = message || "";
    this.args = [];
    this.pre_hook = null;
    this.post_hook = null;
    this.handler = null;
    this.subcommands = [];
    this.is_pre_hook = false;
  }
  addArgString(n, arg2, arg3) {
    // Match Qt client: addArgString(name, required: bool, desc) OR addArgString(name, desc: string, defaultValue)
    if (typeof arg2 === 'boolean') {
      this.args.push({ name: n, type: 'STRING', required: arg2, description: arg3 || '', mark: '', flag: false, defaultUsed: false, defaultValue: undefined });
    } else {
      this.args.push({ name: n, type: 'STRING', required: true, description: arg2 || '', mark: '', flag: false, defaultUsed: arg3 !== undefined, defaultValue: arg3 });
    }
    return this;
  }
  addArgInt(n, arg2, arg3) {
    if (typeof arg2 === 'boolean') {
      this.args.push({ name: n, type: 'INT', required: arg2, description: arg3 || '', mark: '', flag: false, defaultUsed: false, defaultValue: undefined });
    } else {
      this.args.push({ name: n, type: 'INT', required: true, description: arg2 || '', mark: '', flag: false, defaultUsed: arg3 !== undefined, defaultValue: arg3 });
    }
    return this;
  }
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
