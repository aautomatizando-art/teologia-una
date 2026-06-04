/**
 * ═══════════════════════════════════════════════════════════
 *  ESP32 Encoder Relay Server  v2.0  —  Inverted Architecture
 * ═══════════════════════════════════════════════════════════
 *
 *  Arquitetura (sem dependência de máquina local):
 *    ESP32  →  wss://relay.example.com/esp32   (ESP32 conecta ao relay)
 *    Browser → wss://relay.example.com/        (browser conecta ao relay)
 *    Relay bridge os dois em cloud (Railway / Render / Fly.io)
 *
 *  Deploy no Railway:
 *    1. Crie um novo projeto no railway.app
 *    2. Deploy este diretório (encoder/relay/)
 *    3. Copie a URL pública (ex: https://abc.up.railway.app)
 *    4. No ESP32: altere RELAY_HOST para "abc.up.railway.app"
 *    5. Na dashboard: cole wss://abc.up.railway.app no campo "Relay URL"
 *
 *  Paths WebSocket:
 *    /esp32   → ESP32 conecta aqui (envia JSON 200ms, recebe comandos setout)
 *    /        → Browser conecta aqui (recebe JSON, envia comandos)
 *
 *  Variáveis de ambiente:
 *    PORT    Porta local (Railway injeta automaticamente)
 * ═══════════════════════════════════════════════════════════
 */

const http = require('http');
const { WebSocket, WebSocketServer } = require('ws');

const PORT = parseInt(process.env.PORT || '3001');

// ─── HTTP server base (Railway exige HTTP para health-check) ──
const httpServer = http.createServer((req, res) => {
  if (req.method === 'GET' && (req.url === '/' || req.url === '/health')) {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({
      status: 'ok',
      relay: 'ESP32 Encoder Relay v2.0',
      esp32_connected: esp32Socket !== null,
      browser_clients: browserCount(),
      ts: Date.now(),
    }));
  } else {
    res.writeHead(404);
    res.end();
  }
});

// ─── WebSocket server (compartilha a mesma porta HTTP) ─────
const wss = new WebSocketServer({ server: httpServer });

let esp32Socket = null;   // única conexão ESP32

function browserCount() {
  let n = 0;
  wss.clients.forEach(c => { if (c._isBrowser && c.readyState === WebSocket.OPEN) n++; });
  return n;
}

function broadcastBrowsers(msg) {
  const payload = typeof msg === 'string' ? msg : JSON.stringify(msg);
  wss.clients.forEach(c => {
    if (c._isBrowser && c.readyState === WebSocket.OPEN) c.send(payload);
  });
}

function relayStatus(connected) {
  broadcastBrowsers({
    __relay: true,
    esp32_connected: connected,
    clients: browserCount(),
    ts: Date.now(),
  });
}

// ─── Roteamento por path ───────────────────────────────────
wss.on('connection', (ws, req) => {
  const url = req.url || '/';

  if (url === '/esp32') {
    // ── Conexão do ESP32 ──────────────────────────────────
    if (esp32Socket && esp32Socket.readyState === WebSocket.OPEN) {
      console.log('[ESP32] Nova conexão — substituindo anterior');
      esp32Socket.terminate();
    }
    esp32Socket = ws;
    ws._isEsp32 = true;
    console.log('[ESP32] ✓ Conectado');
    relayStatus(true);

    ws.on('message', data => {
      // Encaminha JSON do ESP32 para todos os browsers
      broadcastBrowsers(data.toString());
    });

    ws.on('close', () => {
      if (esp32Socket === ws) esp32Socket = null;
      console.log('[ESP32] Desconectado');
      relayStatus(false);
    });

    ws.on('error', err => {
      console.error('[ESP32] Erro:', err.message);
    });

  } else {
    // ── Conexão do Browser ────────────────────────────────
    ws._isBrowser = true;
    const addr = req.socket.remoteAddress;
    console.log(`[Browser] + ${addr}  (total: ${browserCount()})`);

    // Envia status atual ao conectar
    ws.send(JSON.stringify({
      __relay: true,
      esp32_connected: esp32Socket !== null && esp32Socket.readyState === WebSocket.OPEN,
      clients: browserCount(),
      ts: Date.now(),
    }));

    ws.on('message', data => {
      // Encaminha comandos do browser para o ESP32
      if (esp32Socket && esp32Socket.readyState === WebSocket.OPEN) {
        esp32Socket.send(data.toString());
      } else {
        ws.send(JSON.stringify({ __relay: true, esp32_connected: false, ts: Date.now() }));
      }
    });

    ws.on('close', () => {
      console.log(`[Browser] - ${addr}  (total: ${browserCount()})`);
    });

    ws.on('error', () => {});
  }
});

// ─── Start ─────────────────────────────────────────────────
httpServer.listen(PORT, () => {
  console.log('\n' + '═'.repeat(52));
  console.log('  ESP32 Encoder Relay  v2.0  (inverted)');
  console.log('  Local  → http://localhost:' + PORT);
  console.log('  ESP32  → wss://<relay-host>/esp32');
  console.log('  Browser→ wss://<relay-host>/');
  console.log('═'.repeat(52));
  console.log('\n  Deploy no Railway:');
  console.log('    1. railway.app → New Project → Deploy from GitHub');
  console.log('    2. Selecione o diretório encoder/relay/');
  console.log('    3. Railway injeta PORT automaticamente');
  console.log('    4. Copie o URL público e configure no ESP32 e dashboard\n');
});

httpServer.on('error', err => {
  console.error('[Relay] Erro no servidor HTTP:', err.message);
  process.exit(1);
});
