#!/usr/bin/env python3
"""
tactical_library.json 重新分类脚本
基于渗透阶段 (Kill Chain) + 问题场景 双重维度分类
"""

import json

# 读取现有 JSON
with open('/Users/blackman/.adaptix/data/tactical_library.json', 'r', encoding='utf-8') as f:
    data = json.load(f)

# 收集所有命令
all_commands = []

for top_item in data:
    if top_item.get('type') == 'category' and not top_item.get('parent_id'):
        for child in top_item.get('children', []):
            if child.get('type') == 'category':
                category_name = child.get('name', '')
                category_id = child.get('id', '')
                for cmd in child.get('children', []):
                    if cmd.get('type') == 'command':
                        cmd_info = {
                            'command': cmd.get('command', {}),
                            'description': cmd.get('description', ''),
                            'id': cmd.get('id', ''),
                            'name': cmd.get('name', ''),
                            'parent_id': cmd.get('parent_id', ''),
                            'type': cmd.get('type', ''),
                            'source_category': category_name,
                            'source_parent_id': category_id
                        }
                        all_commands.append(cmd_info)

print(f"共收集到 {len(all_commands)} 条命令")

# 定义新的分类映射规则
# 基于命令关键词映射到新分类
classification_rules = {
    # 初始访问 / Payload 生成
    'initial_access': [
        'msfvenom', 'payload', 'shell', 'reverse', 'meterpreter', 'exploit'
    ],
    # 代码执行
    'execution': [
        'powershell', 'cmd', 'shell', 'execute', 'run', 'bash', 'python', 'perl', 'ruby', 'php'
    ],
    # 权限提升
    'privesc': [
        'privesc', 'privilege', 'escalation', 'sudo', 'getuid', 'whoami', 'token'
    ],
    # 持久化
    'persistence': [
        'persist', 'registry', 'schtasks', 'cron', 'service', 'startup', 'runkey', 'backdoor'
    ],
    # 防御绕过 / EDR 绕过
    'defense_evasion': [
        'edr', 'bypass', 'av', 'amsi', 'rundll32', 'mshta', 'regsvr32', 'cmstp', 
        'msbuild', 'installutil', 'certutil', 'lolbas', 'obfuscate', 'encode', 'encrypt'
    ],
    # 凭证获取
    'credential_access': [
        'mimikatz', 'credential', 'hash', 'pth', 'pass', 'key', 'ticket', 'kerberos',
        'sam', 'lsass', 'creds', 'dump', 'secrets', 'golden ticket', 'silver ticket'
    ],
    # 横向移动
    'lateral_movement': [
        'psexec', 'wmiexec', 'smbexec', 'dcomexec', 'atexec', 'impacket', 
        'lateral', 'move', 'remote', 'rdp', 'winrm', 'ssh', 'crackmapexec'
    ],
    # 信息收集 / 发现
    'discovery': [
        'discover', 'enum', 'nmap', 'scan', 'recon', 'info', 'systeminfo', 'hostname',
        'process', 'ps', 'netstat', 'ipconfig', 'arp', 'route', 'bloodhound', 'ldapsearch',
        'enum4linux', 'searchsploit', 'rustscan'
    ],
    # 数据收集与外泄
    'collection_exfil': [
        'download', 'upload', 'exfil', 'collect', 'screenshot', 'clipboard', 'keylog'
    ],
    # 隧道与代理
    'tunneling': [
        'tunnel', 'proxy', 'socks', 'portfwd', 'lportfwd', 'rportfwd', 'chisel', 
        'ssh', 'plink', 'nc', 'netcat', 'forward'
    ],
    # Web 应用攻击
    'web_attacks': [
        'xss', 'sqli', 'sql', 'injection', 'lfi', 'rfi', 'csrf', 'ssrf', 'xxe'
    ],
    # 编码与加密
    'encoding_crypto': [
        'encode', 'decode', 'base64', 'hex', 'hash', 'md5', 'sha', 'crypt', 'cert'
    ],
    # 文件操作
    'file_operations': [
        'ls', 'dir', 'cat', 'read', 'write', 'copy', 'move', 'rm', 'delete', 
        'mkdir', 'zip', 'tar', 'download', 'upload'
    ],
    # 网络工具
    'network_tools': [
        'ping', 'curl', 'wget', 'nc', 'netcat', 'dns', 'whois', 'traceroute'
    ],
    # 反向 Shell
    'reverse_shell': [
        'reverse', 'revshell', 'listener', 'connectback'
    ]
}

def classify_command(cmd_info):
    """根据命令特征分类"""
    cmd_text = ''
    if 'command' in cmd_info:
        cmd_obj = cmd_info['command']
        if isinstance(cmd_obj, dict):
            cmd_text = cmd_obj.get('cmd', '') + ' ' + cmd_obj.get('description', '')
        else:
            cmd_text = str(cmd_obj)
    cmd_text += ' ' + cmd_info.get('description', '') + ' ' + cmd_info.get('name', '')
    cmd_text = cmd_text.lower()
    
    for category, keywords in classification_rules.items():
        for keyword in keywords:
            if keyword in cmd_text:
                return category
    return 'utilities'

# 分类命令
categorized = {k: [] for k in classification_rules.keys()}
categorized['utilities'] = []

for cmd in all_commands:
    category = classify_command(cmd)
    categorized[category].append(cmd)

# 打印分类结果
for cat, cmds in categorized.items():
    print(f"{cat}: {len(cmds)} 条命令")

# 生成新的 JSON 结构
new_structure = []

# 定义分类顺序和详细信息
category_order = [
    ('initial_access', '初始访问 (Initial Access)', 'Payload 生成与初始访问技术'),
    ('reverse_shell', '反向 Shell (Reverse Shell)', '各类反向 Shell 生成与连接'),
    ('execution', '代码执行 (Execution)', '系统命令与代码执行技术'),
    ('defense_evasion', '防御绕过 (Defense Evasion)', 'EDR/AV 绕过与规避技术'),
    ('persistence', '持久化 (Persistence)', '后门植入与持久化技术'),
    ('privesc', '权限提升 (Privilege Escalation)', '提权技术与权限获取'),
    ('credential_access', '凭证获取 (Credential Access)', '凭证转储与票据攻击'),
    ('lateral_movement', '横向移动 (Lateral Movement)', '跨系统移动技术'),
    ('discovery', '信息收集 (Discovery)', '主机与网络发现技术'),
    ('collection_exfil', '数据收集与外泄 (Collection & Exfil)', '数据窃取与外泄技术'),
    ('tunneling', '隧道与代理 (Tunneling)', '网络隧道与代理技术'),
    ('web_attacks', 'Web 应用攻击 (Web Attacks)', 'Web 漏洞利用技术'),
    ('encoding_crypto', '编码与加密 (Encoding & Crypto)', '编码解码与加密工具'),
    ('file_operations', '文件操作 (File Operations)', '文件系统操作命令'),
    ('network_tools', '网络工具 (Network Tools)', '网络诊断工具'),
    ('utilities', '辅助工具 (Utilities)', '其他实用工具'),
]

# 生成新结构
for cat_id, cat_name, cat_desc in category_order:
    if categorized[cat_id]:
        category_item = {
            "children": [],
            "description": cat_desc,
            "id": f"cat_{cat_id}",
            "name": cat_name,
            "parent_id": "cat_tactical",
            "type": "category"
        }
        
        for cmd in categorized[cat_id]:
            cmd_obj = cmd.get('command', {})
            category_item["children"].append({
                "command": cmd_obj if isinstance(cmd_obj, dict) else {},
                "description": cmd.get('description', ''),
                "id": cmd.get('id', ''),
                "name": cmd.get('name', ''),
                "parent_id": f"cat_{cat_id}",
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

# 输出统计
print(f"\n新分类结构:")
print(f"  顶级分类: 1 (cat_tactical)")
print(f"  子分类数: {len(new_structure)}")
total_cmds = sum(len(cat['children']) for cat in new_structure)
print(f"  命令总数: {total_cmds}")

# 保存
output_path = '/Users/blackman/.adaptix/data/tactical_library.json.new'
with open(output_path, 'w', encoding='utf-8') as f:
    json.dump([root], f, ensure_ascii=False, indent=4)

print(f"\n已保存到: {output_path}")
