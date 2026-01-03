#!/bin/bash

# 获取当前脚本所在目录的绝对路径
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
NGINX_PREFIX="$SCRIPT_DIR"

echo "Starting Production-Ready Nginx Server (Local VPS Mode)..."
echo "Project Root: $PROJECT_ROOT"
echo "Config: $SCRIPT_DIR/adaptix_nginx.conf"

# 检查 Nginx 是否已安装
if ! command -v nginx &> /dev/null; then
    echo "Error: nginx command not found. Please install nginx first."
    exit 1
fi

# 检查证书
CERT_CRT="$PROJECT_ROOT/release/server.rsa.crt"
if [ ! -f "$CERT_CRT" ]; then
    echo "Error: Certificate not found at $CERT_CRT"
    exit 1
fi

# 准备日志和临时目录
mkdir -p "$SCRIPT_DIR/logs"
mkdir -p "$SCRIPT_DIR/temp/client_body"
mkdir -p "$SCRIPT_DIR/temp/proxy"

# 停止可能存在的旧进程
PID_FILE="$SCRIPT_DIR/nginx.pid"
if [ -f "$PID_FILE" ]; then
    echo "Stopping existing instance..."
    nginx -p "$SCRIPT_DIR" -c adaptix_nginx.conf -s stop 2>/dev/null
    sleep 1
fi

# 替换域名占位符为 localhost
sed -i '' "s/YOUR_DOMAIN_OR_IP/localhost/g" "$SCRIPT_DIR/adaptix_nginx.conf" 2>/dev/null || sed -i "s/YOUR_DOMAIN_OR_IP/localhost/g" "$SCRIPT_DIR/adaptix_nginx.conf"

# 启动 Nginx
# 使用 -p 指定前缀，-c 指定配置
nginx -p "$SCRIPT_DIR" -c adaptix_nginx.conf

if [ $? -eq 0 ]; then
    echo "=================================================="
    echo "✅ Production Nginx Started Successfully!"
    echo "=================================================="
    echo "Web Interface: https://localhost"
    echo "API Backend:   https://127.0.0.1:4321 (Proxied)"
    echo "Logs:          $SCRIPT_DIR/access.log"
    echo "=================================================="
else
    echo "❌ Failed to start Nginx. Check error log."
    exit 1
fi
