#!/bin/bash

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 检查是否以 root 运行
if [ "$EUID" -ne 0 ]; then 
  echo -e "${RED}[!] 请使用 sudo 或 root 权限运行此脚本${NC}"
  exit 1
fi

echo -e "${GREEN}[*] 开始 AdaptixC2 Web Client 生产环境部署...${NC}"

# 1. 基础环境安装
echo -e "${YELLOW}[*] 安装 Nginx 和必要的工具...${NC}"
if [ -f /etc/debian_version ]; then
    apt-get update
    apt-get install -y nginx openssl ufw tar
elif [ -f /etc/redhat-release ]; then
    yum install -y nginx openssl ufw tar
else
    echo -e "${RED}[!] 不支持的操作系统，请手动安装 Nginx${NC}"
    exit 1
fi

# 2. 部署 Web Client 文件
echo -e "${YELLOW}[*] 部署 Web Client 静态文件...${NC}"
WEB_ROOT="/var/www/adaptix"
mkdir -p "$WEB_ROOT/dist"

# 检查 web_dist 是否存在
if [ ! -d "web_dist" ]; then
    echo -e "${RED}[!] 找不到 web_dist 目录，请确保脚本在正确的位置运行${NC}"
    exit 1
fi

rm -rf "$WEB_ROOT/dist/*"
cp -r web_dist/* "$WEB_ROOT/dist/"
chown -R www-data:www-data "$WEB_ROOT" 2>/dev/null || chown -R nginx:nginx "$WEB_ROOT"

# 3. 配置 SSL 证书
echo -e "${YELLOW}[*] 配置 SSL 证书...${NC}"
mkdir -p /etc/nginx/ssl
CERT_KEY="/etc/nginx/ssl/adaptix.key"
CERT_CRT="/etc/nginx/ssl/adaptix.crt"

# 询问域名
echo -e "${GREEN}[?] 请输入您的服务器 IP 或域名 (用于生成证书和配置 Nginx):${NC}"
read -p "> " SERVER_NAME

if [ -z "$SERVER_NAME" ]; then
    SERVER_NAME="localhost"
fi

# 生成自签名证书 (作为默认/兜底)
echo -e "${YELLOW}[*] 生成自签名证书 (有效期 3650 天)...${NC}"
openssl req -x509 -nodes -days 3650 -newkey rsa:2048 \
    -keyout "$CERT_KEY" \
    -out "$CERT_CRT" \
    -subj "/C=US/ST=State/L=City/O=Adaptix/OU=Operations/CN=$SERVER_NAME" 2>/dev/null

echo -e "${GREEN}[+] 证书已生成: $CERT_CRT${NC}"

# 4. 配置 Nginx
echo -e "${YELLOW}[*] 应用 Nginx 配置...${NC}"
NGINX_CONF="/etc/nginx/sites-available/adaptix"
NGINX_CONF_LINK="/etc/nginx/sites-enabled/adaptix"

# 复制配置文件
if [ -f "adaptix_nginx.conf" ]; then
    cp adaptix_nginx.conf "$NGINX_CONF"
    # 替换域名
    sed -i "s/YOUR_DOMAIN_OR_IP/$SERVER_NAME/g" "$NGINX_CONF"
else
    echo -e "${RED}[!] 找不到 adaptix_nginx.conf 配置文件${NC}"
    exit 1
fi

# 启用配置 (Debian/Ubuntu 风格)
if [ -d "/etc/nginx/sites-enabled" ]; then
    ln -sf "$NGINX_CONF" "$NGINX_CONF_LINK"
    rm -f /etc/nginx/sites-enabled/default
else
    # CentOS/RHEL 风格
    cp "$NGINX_CONF" /etc/nginx/conf.d/adaptix.conf
    mv /etc/nginx/conf.d/default.conf /etc/nginx/conf.d/default.conf.bak 2>/dev/null
fi

# 测试并重启 Nginx
echo -e "${YELLOW}[*] 测试 Nginx 配置...${NC}"
nginx -t
if [ $? -eq 0 ]; then
    systemctl restart nginx
    echo -e "${GREEN}[+] Nginx 已重启${NC}"
else
    echo -e "${RED}[!] Nginx 配置测试失败，请检查错误日志${NC}"
    exit 1
fi

# 5. 配置防火墙
echo -e "${YELLOW}[*] 配置 UFW 防火墙...${NC}"
# 尝试启用 UFW
if command -v ufw >/dev/null; then
    ufw allow ssh
    ufw allow 80
    ufw allow 443
    # 如果 Teamserver 在本地，且希望通过 4321 调试（可选，生产环境建议关闭）
    # ufw allow 4321 
    
    echo -e "${GREEN}[+] 防火墙规则已更新 (SSH, 80, 443 已开放)${NC}"
    echo -e "${YELLOW}[!] 注意: 请确保您的云服务商防火墙 (Security Group) 也开放了这些端口${NC}"
else
    echo -e "${YELLOW}[-] 未检测到 UFW，跳过防火墙配置${NC}"
fi

echo -e "\n${GREEN}==============================================${NC}"
echo -e "${GREEN}   ✅ 部署完成！${NC}"
echo -e "${GREEN}==============================================${NC}"
echo -e "Web Client 访问地址: ${YELLOW}https://$SERVER_NAME${NC}"
echo -e "API 后端地址:        ${YELLOW}https://127.0.0.1:4321${NC} (已通过 Nginx 隐蔽转发)"
echo -e ""
echo -e "注意: 由于使用自签名证书，浏览器首次访问会提示不安全。"
echo -e "请点击 '高级' -> '继续访问' (Chrome) 或 '接受风险并继续' (Firefox)。"
echo -e "如果拥有域名，建议后续使用 Certbot 安装可信证书。"
echo -e "${GREEN}==============================================${NC}"
