#!/usr/bin/env python3
"""验证命令分类是否正确"""

import json

with open('/Users/blackman/.adaptix/data/tactical_library.json', 'r', encoding='utf-8') as f:
    data = json.load(f)

# 收集所有命令及其分类
commands_by_category = {}

for item in data:
    if item.get('id') == 'cat_tactical':
        for child in item['children']:
            cat_name = child.get('name', '')
            for cmd in child.get('children', []):
                if cmd.get('type') == 'command':
                    cmd_name = cmd.get('name', '')
                    cmd_desc = cmd.get('description', '')
                    cmd_id = cmd.get('id', '')
                    
                    if cat_name not in commands_by_category:
                        commands_by_category[cat_name] = []
                    
                    commands_by_category[cat_name].append({
                        'name': cmd_name,
                        'description': cmd_desc,
                        'id': cmd_id
                    })

# 定义预期的分类规则
expected_classifications = {
    '初始访问 (Initial Access)': ['msfvenom', 'payload', 'generate', 'shellcode', 'exploit'],
    '反向 Shell (Reverse Shell)': ['reverse', 'revshell', 'connectback', 'listener'],
    '代码执行 (Execution)': ['powershell', 'cmd', 'shell', 'execute', 'run', 'bash', 'python', 'perl', 'php'],
    '防御绕过 (Defense Evasion)': ['bypass', 'edr', 'av', 'amsi', 'rundll32', 'mshta', 'regsvr32', 'cmstp', 'msbuild', 'certutil', 'obfuscate', 'encode'],
    '持久化 (Persistence)': ['persist', 'registry', 'schtasks', 'cron', 'service', 'startup', 'runkey', 'backdoor'],
    '权限提升 (Privilege Escalation)': ['privesc', 'privilege', 'escalation', 'sudo', 'getuid', 'whoami', 'token'],
    '凭证获取 (Credential Access)': ['mimikatz', 'credential', 'hash', 'pth', 'pass', 'key', 'ticket', 'kerberos', 'sam', 'lsass', 'golden ticket'],
    '横向移动 (Lateral Movement)': ['psexec', 'wmiexec', 'smbexec', 'dcomexec', 'atexec', 'lateral', 'winrm', 'crackmapexec'],
    '信息收集 (Discovery)': ['discover', 'enum', 'nmap', 'scan', 'recon', 'systeminfo', 'hostname', 'process', 'ps', 'netstat', 'bloodhound', 'ldapsearch'],
    '数据收集与外泄 (Collection & Exfil)': ['download', 'upload', 'exfil', 'collect', 'screenshot', 'clipboard'],
    '隧道与代理 (Tunneling)': ['tunnel', 'proxy', 'socks', 'portfwd', 'chisel', 'plink'],
    'Web 应用攻击 (Web Attacks)': ['xss', 'sqli', 'sql', 'injection', 'lfi', 'rfi', 'csrf', 'ssrf'],
    '编码与加密 (Encoding & Crypto)': ['encode', 'decode', 'base64', 'hex', 'hash', 'md5', 'sha', 'crypt'],
    '文件操作 (File Operations)': ['ls', 'dir', 'cat', 'read', 'write', 'copy', 'move', 'rm', 'mkdir', 'zip'],
    '网络工具 (Network Tools)': ['ping', 'curl', 'wget', 'nc', 'netcat', 'dns', 'whois'],
    '辅助工具 (Utilities)': []
}

# 检查分类
print("=== 分类验证报告 ===\n")

issues = []
correct = 0
total = 0

for cat_name, cmds in commands_by_category.items():
    for cmd in cmds:
        total += 1
        cmd_text = (cmd['name'] + ' ' + cmd['description']).lower()
        
        # 检查是否在正确的分类
        found_correct = False
        for expected_cat, keywords in expected_classifications.items():
            if keywords:  # 跳过 Utilities
                for kw in keywords:
                    if kw in cmd_text:
                        if expected_cat == cat_name:
                            found_correct = True
                        else:
                            issues.append(f"可能分类错误: '{cmd['name']}' 在 '{cat_name}'，但包含 '{kw}'")
                        break
        
        if found_correct:
            correct += 1

print(f"总命令数: {total}")
print(f"正确分类: {correct}")
print(f"可能问题: {len(issues)}")

if issues:
    print("\n=== 可能的问题 ===")
    for issue in issues[:20]:  # 只显示前20个
        print(f"  - {issue}")

# 打印各分类命令数
print("\n=== 各分类命令数 ===")
for cat_name, cmds in sorted(commands_by_category.items(), key=lambda x: -len(x[1])):
    print(f"  {cat_name}: {len(cmds)} 条")
