// Minimal mock of the gateway backend: serves the built UI + the few REST endpoints the layout needs.
// Usage: node tests/mock-ui-server.mjs interface/build tests/test-data-overview-card-states.json 4173
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
const [,, buildDir, fixture, port] = process.argv;
const devices = JSON.parse(fs.readFileSync(fixture, 'utf8'));
const types = { '.html':'text/html', '.js':'text/javascript', '.css':'text/css', '.json':'application/json', '.svg':'image/svg+xml', '.png':'image/png', '.ico':'image/x-icon', '.woff2':'font/woff2' };
const json = (res, obj, status = 200) => { res.writeHead(status, {'Content-Type':'application/json'}); res.end(JSON.stringify(obj)); };
const puts = [];
http.createServer((req, res) => {
  const url = new URL(req.url, 'http://x');
  const p = url.pathname;
  if (p === '/rest/features') return json(res, { security:true, mqtt:true, ntp:true, upload_firmware:true, download_firmware:true, sleep:false, battery:false, analytics:false, event_use_json:true, coredump:true, home_assistant:true });
  if (p === '/rest/verifyAuthorization') return json(res, {});
  if (p === '/rest/gateway-devices') return setTimeout(() => json(res, devices), 600); // delayed: deep link must wait for load
  if (p === '/rest/gateway-devices/device' && req.method === 'PUT') {
    let body = ''; req.on('data', c => body += c);
    return req.on('end', () => { const d = JSON.parse(body); puts.push(d); const list = devices.devices || devices; const i = list.findIndex(x => x.id === d.id); if (i >= 0) list[i] = d; json(res, d); });
  }
  if (p === '/__puts') return json(res, puts);
  if (p === '/rest/alarm-lines') return json(res, { lines: [] });
  if (p === '/rest/cc1101/status') return json(res, { state: 'running' });
  if (p.startsWith('/rest/') || p.startsWith('/ws/')) return json(res, {});
  let f = path.join(buildDir, p === '/' ? 'index.html' : p);
  if (!fs.existsSync(f) || fs.statSync(f).isDirectory()) f = path.join(buildDir, 'index.html'); // SPA fallback
  res.writeHead(200, {'Content-Type': types[path.extname(f)] || 'application/octet-stream'});
  fs.createReadStream(f).pipe(res);
}).listen(+port, () => console.log('mock on', port));
