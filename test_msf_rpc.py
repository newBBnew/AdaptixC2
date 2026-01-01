import requests
import msgpack
import urllib3

# Disable SSL warnings for local test
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

class MsfRpcClient:
    def __init__(self, host='127.0.0.1', port=55553, username='msf', password='test', use_ssl=False):
        protocol = "https" if use_ssl else "http"
        self.url = f"{protocol}://{host}:{port}/api/"
        self.username = username
        self.password = password
        self.token = None

    def call(self, method, *args):
        params = [method]
        if self.token:
            params.append(self.token)
        params.extend(args)
        
        payload = msgpack.packb(params)
        headers = {'Content-Type': 'binary/message-pack'}
        
        try:
            response = requests.post(self.url, data=payload, headers=headers, verify=False)
            if response.status_code == 200:
                return msgpack.unpackb(response.content, raw=False)
            else:
                print(f"[-] RPC Error: {response.status_code}")
                print(f"[-] Response Content: {response.content}")
                return None
        except Exception as e:
            print(f"[-] Request Error: {e}")
            return None

    def login(self):
        # The msfrpcd started with -P test uses 'msf' as default username
        res = self.call('auth.login', self.username, self.password)
        print(f"[*] auth.login response for {self.username}: {res}")
        if res:
            # Handle both byte keys and string keys
            result = res.get('result') or res.get(b'result')
            token = res.get('token') or res.get(b'token')
            
            if (isinstance(result, bytes) and result == b'success') or result == 'success':
                self.token = token.decode() if isinstance(token, bytes) else token
                return True
        return False

if __name__ == "__main__":
    host = '127.0.0.1'
    port = 55554
    username = 'msf'
    password = 'test'
    
    client = MsfRpcClient(host=host, port=port, username=username, password=password, use_ssl=False)
    print(f"[*] Attempting login to MSF RPC on {host}:{port}...")
    
    if client.login():
        print(f"[+] Login successful!")
        
        # Capability Check 1: Core Version
        version = client.call('core.version')
        print(f"[+] Version: {version.get('version') if version else 'Error'}")
        
        # Capability Check 2: Module List (Exploits)
        print("[*] Checking module access (exploits)...")
        exploits = client.call('module.exploits')
        if exploits and 'modules' in exploits:
            print(f"[+] Successfully retrieved {len(exploits['modules'])} exploits.")
        
        # Capability Check 3: Sessions
        print("[*] Checking session management...")
        sessions = client.call('session.list')
        print(f"[+] Active Sessions: {sessions}")
        
        # Capability Check 4: Plugin loading (Check if we can interact with more than just core)
        print("[*] Checking plugin status...")
        plugins = client.call('core.plugins')
        print(f"[+] Loaded Plugins: {plugins}")
        
        print("\n[!] MSF RPC appears to be fully functional for remote control.")
    else:
        print(f"[-] Login failed. Please check msfrpcd status.")
