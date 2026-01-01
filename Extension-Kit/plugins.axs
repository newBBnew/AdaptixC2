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

/// ELEVATION
ax.plugin_register("Elevation", "getsystem token", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Elevation", "uacbybass sspi", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Elevation", "uacbybass regshellcmd", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Elevation", "potato-dcom", ["beacon", "gopher"], ["windows"]);
ax.plugin_register("Elevation", "potato-print", ["beacon", "gopher"], ["windows"]);

ax.log("[Plugins] Registered " + ax.plugin_list().length + " commands to Plugins menu");
