#!/usr/bin/env python3
"""智能重新分类脚本 - 修复分类问题"""

import json

with open('/Users/blackman/.adaptix/data/tactical_library.json', 'r', encoding='utf-8') as f:
    data = json.load(f)

# 收集所有命令
all_commands = []

for item in data:
    if item.get('id') == 'cat_tactical':
        for child in item['children']:
            for cmd in child.get('children', []):
                if cmd.get('type') == 'command':
                    all_commands.append({
                        'command': cmd.get('command', {}),
                        'description': cmd.get('description', ''),
                        'id': cmd.get('id', ''),
                        'name': cmd.get('name', ''),
                        'parent_id': cmd.get('parent_id', ''),
                        'type': cmd.get('type', '')
                    })

print(f"收集到 {len(all_commands)} 条命令")

# 改进的分类规则 - 按优先级排序
classification_rules = [
    # 最高优先级: 反向 Shell
    ('反向 Shell (Reverse Shell)', [
        ('reverse shell', 100),
        ('reverse_shell', 100),
        ('revshell', 100),
        ('connectback', 100),
        ('php reverse', 100),
        ('python reverse', 100),
        ('perl reverse', 100),
        ('ruby reverse', 100),
        ('bash reverse', 100),
        ('nc reverse', 100),
        ('netcat reverse', 100),
        ('msfvenom.*reverse', 90),
    ]),
    # 高优先级: Web 攻击
    ('Web 应用攻击 (Web Attacks)', [
        ('xss', 100),
        ('sqli', 100),
        ('sql injection', 100),
        ('sql injection', 100),
        ('lfi', 100),
        ('rfi', 100),
        ('csrf', 100),
        ('ssrf', 100),
        ('xxe', 100),
        ('ssti', 100),
    ]),
    # 高优先级: 凭证获取
    ('凭证获取 (Credential Access)', [
        ('mimikatz', 100),
        ('credential', 90),
        ('hash dump', 100),
        ('pth', 100),
        ('pass the hash', 100),
        ('golden ticket', 100),
        ('silver ticket', 100),
        ('kerberos', 90),
        ('lsass', 100),
        ('sam database', 100),
        ('sekurlsa', 100),
        ('lsadump', 100),
        ('kerberos::golden', 100),
    ]),
    # 高优先级: 横向移动
    ('横向移动 (Lateral Movement)', [
        ('psexec', 100),
        ('wmiexec', 100),
        ('smbexec', 100),
        ('dcomexec', 100),
        ('atexec', 100),
        ('impacket', 90),
        ('crackmapexec', 90),
        ('evil-winrm', 90),
        ('winrm', 80),
        ('rdp', 80),
        ('psexec.exe', 100),
    ]),
    # 高优先级: 持久化
    ('持久化 (Persistence)', [
        ('registry', 90),
        ('schtasks', 100),
        ('cron', 100),
        ('service', 80),
        ('startup', 90),
        ('runkey', 100),
        ('backdoor', 100),
        ('persist', 90),
        ('autorun', 100),
        ('registry add', 100),
        ('registry set', 100),
    ]),
    # 高优先级: 防御绕过
    ('防御绕过 (Defense Evasion)', [
        ('edr bypass', 100),
        ('bypass', 80),
        ('rundll32', 90),
        ('mshta', 100),
        ('regsvr32', 100),
        ('cmstp', 100),
        ('msbuild', 100),
        ('installutil', 100),
        ('odbcconf', 100),
        ('desktopimgdownldr', 100),
        ('printbrm', 100),
        ('addinutil', 100),
        ('ieexec', 100),
        ('presentationhost', 100),
        ('dfsvc', 100),
        ('amsi', 100),
        ('obfuscate', 90),
        ('lolbas', 90),
    ]),
    # 中优先级: 隧道代理
    ('隧道与代理 (Tunneling)', [
        ('socks', 100),
        ('portfwd', 100),
        ('lportfwd', 100),
        ('rportfwd', 100),
        ('chisel', 100),
        ('ssh', 80),
        ('plink', 100),
        ('nc -l', 100),
        ('netcat -l', 100),
        ('tunnel', 90),
        ('proxy', 90),
    ]),
    # 中优先级: 编码加密
    ('编码与加密 (Encoding & Crypto)', [
        ('base64 encode', 100),
        ('base64 decode', 100),
        ('hex encode', 100),
        ('hex decode', 100),
        ('encodehex', 100),
        ('certutil', 80),
        ('hash', 70),
        ('md5', 80),
        ('sha', 80),
        ('encrypt', 80),
        ('decrypt', 80),
    ]),
    # 中优先级: 权限提升
    ('权限提升 (Privilege Escalation)', [
        ('privesc', 100),
        ('privilege escalation', 100),
        ('getuid', 100),
        ('whoami /all', 100),
        ('token', 80),
        ('sudo', 100),
        ('uac', 100),
        ('bypassuac', 100),
    ]),
    # 中优先级: 数据收集
    ('数据收集与外泄 (Collection & Exfil)', [
        ('screenshot', 100),
        ('exfil', 100),
        ('clipboard', 100),
        ('keylog', 100),
        ('collect', 90),
    ]),
    # 中优先级: 信息收集
    ('信息收集 (Discovery)', [
        ('nmap', 100),
        ('scan', 80),
        ('enum', 80),
        ('bloodhound', 100),
        ('ldapsearch', 100),
        ('enum4linux', 100),
        ('rustscan', 100),
        ('searchsploit', 100),
        ('systeminfo', 100),
        ('netstat', 100),
        ('ipconfig', 100),
        ('arp', 100),
        ('hostname', 100),
    ]),
    # 低优先级: 代码执行
    ('代码执行 (Execution)', [
        ('powershell', 70),
        ('cmd.exe', 80),
        ('cmd /c', 80),
        ('execute', 80),
        ('shell', 70),
        ('bash', 80),
        ('python', 80),
        ('perl', 80),
        ('ruby', 80),
        ('php', 80),
        ('wmic', 100),
        ('cscript', 100),
        ('wscript', 100),
        ('runas', 100),
    ]),
    # 低优先级: 初始访问
    ('初始访问 (Initial Access)', [
        ('msfvenom', 100),
        ('payload', 90),
        ('shellcode', 100),
        ('exploit', 90),
        ('generate', 80),
        ('download', 70),
        ('upload', 70),
    ]),
    # 文件操作
    ('文件操作 (File Operations)', [
        ('ls ', 90),
        ('dir ', 90),
        ('cat ', 90),
        ('copy', 80),
        ('move', 80),
        ('rm ', 90),
        ('mkdir', 90),
        ('zip', 90),
        ('tar', 90),
        ('download', 60),
        ('upload', 60),
    ]),
    # 网络工具
    ('网络工具 (Network Tools)', [
        ('ping', 100),
        ('curl', 100),
        ('wget', 100),
        ('whois', 100),
        ('traceroute', 100),
        ('dns', 80),
    ]),
]

def classify_command(cmd_info):
    """智能分类命令"""
    cmd_text = cmd_info.get('name', '') + ' ' + cmd_info.get('description', '')
    cmd_text = cmd_text.lower()
    
    best_category = None
    best_score = 0
    
    for category, rules in classification_rules:
        for pattern, score in rules:
            if pattern in cmd_text:
                if score > best_score:
                    best_score = score
                    best_category = category
    
    return best_category if best_category else '辅助工具 (Utilities)'

# 重新分类
categorized = {}
for cmd in all_commands:
    cat = classify_command(cmd)
    if cat not in categorized:
        categorized[cat] = []
    categorized[cat].append(cmd)

# 打印新分类
print("\n=== 重新分类结果 ===")
total = 0
for cat, cmds in sorted(categorized.items(), key=lambda x: -len(x[1])):
    print(f"{cat}: {len(cmds)} 条")
    total += len(cmds)
print(f"\n总计: {total} 条命令")

# 生成新的 JSON 结构
new_structure = []
category_order = [
    '初始访问 (Initial Access)',
    '反向 Shell (Reverse Shell)',
    '代码执行 (Execution)',
    '防御绕过 (Defense Evasion)',
    '持久化 (Persistence)',
    '权限提升 (Privilege Escalation)',
    '凭证获取 (Credential Access)',
    '横向移动 (Lateral Movement)',
    '信息收集 (Discovery)',
    '数据收集与外泄 (Collection & Exfil)',
    '隧道与代理 (Tunneling)',
    'Web 应用攻击 (Web Attacks)',
    '编码与加密 (Encoding & Crypto)',
    '文件操作 (File Operations)',
    '网络工具 (Network Tools)',
    '辅助工具 (Utilities)',
]

for cat_name in category_order:
    if cat_name in categorized:
        cmds = categorized[cat_name]
        cat_id = 'cat_' + cat_name.split('(')[1].split(')')[0].lower().replace(' ', '_')
        
        category_item = {
            "children": [],
            "description": cat_name.split('(')[0].strip(),
            "id": cat_id,
            "name": cat_name,
            "parent_id": "cat_tactical",
            "type": "category"
        }
        
        for cmd in cmds:
            cmd_obj = cmd.get('command', {})
            category_item["children"].append({
                "command": cmd_obj if isinstance(cmd_obj, dict) else {},
                "description": cmd.get('description', ''),
                "id": cmd.get('id', ''),
                "name": cmd.get('name', ''),
                "parent_id": cat_id,
                "type": "command"
            })
        
        new_structure.append(category_item)

# 添加根节点
root = {
    "children": new_structure,
    "description": "战术命令库 - 按渗透阶段分类",
    "id": "cat_tactical",
    "name": "战术命令库 (Tactical Commands)",
    "parent_id": "",
    "type": "category"
}

# 保存
output_path = '/Users/blackman/.adaptix/data/tactical_library.json'
with open(output_path, 'w', encoding='utf-8') as f:
    json.dump([root], f, ensure_ascii=False, indent=4)

print(f"\n已保存到: {output_path}")
