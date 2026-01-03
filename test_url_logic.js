const inputUrl = 'https://localhost:4321/endpoint';
const normalizedUrl = inputUrl.trim();
const urlObj = new URL(normalizedUrl);
const origin = urlObj.origin;
let path = urlObj.pathname;

console.log('Original path:', path);

if (path.length > 1 && path.endsWith('/')) {
path = path.slice(0, -1);
}

console.log('Processed path:', path);

const isLocal = normalizedUrl.includes('localhost') || normalizedUrl.includes('127.0.0.1');
console.log('isLocal:', isLocal);

const endpointPrefix = path === '/' ? '' : path;
console.log('endpointPrefix:', endpointPrefix);

const apiEndpoint = isLocal ? `/api${path === '/' ? '' : path}` : `${origin}${path}`;
console.log('apiEndpoint:', apiEndpoint);

const loginUrl = `${apiEndpoint}/login`;
console.log('loginUrl:', loginUrl);
