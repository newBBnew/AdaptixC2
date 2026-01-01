/**
 * AdaptixC2 Plugin Registration
 * 
 * This file registers Extension-Kit commands to the Plugins menu.
 * Load this file after loading extension-kit.axs
 */

var metadata = {
    name: "Plugins Registration",
    description: "Register Extension-Kit commands to Plugins menu",
    nosave: true
};

/// LATERAL MOVEMENT
ax.plugin_register("Lateral Movement", "jump psexec", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Lateral Movement", "jump scshell", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Lateral Movement", "jump winrm", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Lateral Movement", "jump wmi", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Lateral Movement", "invoke winrm", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Lateral Movement", "invoke scshell", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Lateral Movement", "smb write", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Lateral Movement", "runas", ["beacon", "gopher"], ["windows"]);

/// TOKEN OPERATIONS
ax.plugin_register("Token", "token make", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Token", "token steal", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Token", "token spawn-make", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Token", "token spawn-steal", ["beacon", "gopher"], ["windows"]);

/// CREDENTIALS
ax.plugin_register("Credentials", "askcreds", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Credentials", "autologon", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Credentials", "credman", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Credentials", "get-netntlm", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Credentials", "hashdump", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Credentials", "nanodump", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Credentials", "underlaycopy", ["beacon", "gopher"], ["windows"]);

/// ELEVATION
ax.plugin_register("Elevation", "getsystem token", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Elevation", "uacbybass sspi", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Elevation", "uacbybass regshellcmd", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Elevation", "potato-dcom", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Elevation", "potato-print", ["beacon", "gopher"], ["windows"]);

/// EXECUTION
ax.plugin_register("Execution", "execute-assembly", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Execution", "noconsolation", ["beacon", "gopher"], ["windows"]);

/// INJECTION
ax.plugin_register("Injection", "inject-cfg", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Injection", "inject-sec", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Injection", "inject-poolparty", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Injection", "inject-32to64", ["beacon", "gopher"], ["windows"]);

/// POST EXPLOITATION
ax.plugin_register("Post Exploitation", "firewallrule add", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Post Exploitation", "screenshot_bof", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Post Exploitation", "sauroneye", ["beacon", "gopher"], ["windows"]);

/// PROCESS
ax.plugin_register("Process", "findobj module", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Process", "findobj prochandle", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Process", "process conn", ["beacon", "gopher"], ["windows"]);

/// ACTIVE DIRECTORY
ax.plugin_register("Active Directory", "adwssearch", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Active Directory", "badtakeover", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Active Directory", "dcsync single", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Active Directory", "dcsync all", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Active Directory", "ldapsearch", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Active Directory", "ldapq computers", ["beacon", "gopher"], ["windows"]);

/// SITUATION AWARENESS (LOCAL)
ax.plugin_register("SA Local", "arp", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("SA Local", "cacls", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("SA Local", "dir", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("SA Local", "env", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("SA Local", "ipconfig", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("SA Local", "listdns", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("SA Local", "netstat", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("SA Local", "nslookup", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("SA Local", "privcheck alwayselevated", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("SA Local", "privcheck hijackablepath", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("SA Local", "privcheck tokenpriv", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("SA Local", "privcheck unattendfiles", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("SA Local", "privcheck unquotedsvc", ["beacon", "gopher"], ["windows"]);

/// SITUATION AWARENESS (REMOTE)
ax.plugin_register("SA Remote", "smartscan", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("SA Remote", "taskhound", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("SA Remote", "quser", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("SA Remote", "nbtscan", ["beacon", "gopher"], ["windows"]);

ax.log("[Plugins] Registered " + ax.plugin_list().length + " commands to Plugins menu");
