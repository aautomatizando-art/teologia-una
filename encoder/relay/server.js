/**
 * ═══════════════════════════════════════════════════════════
 *  ESP32 Encoder Relay Server  v1.0
 * ═══════════════════════════════════════════════════════════
 *
 *  Arquitetura:
 *    ESP32 (ws://192.168.x.x:81/ws)  ←→  [este servidor]  ←→  Browser (wss://…)
 *
 *  Como usar:
 *    1. Instalar dependências:
 *         npm install
 *
 *    2. Iniciar o relay (com o IP do ESP32):
 *         ESP32_IP=192.168.1.100 npm start
 *
 *    3. Expor publicamente (escolha um):
 *         cloudflared tunnel --url http://localhost:3001
 *         npx localtunnel --port 3001
 *         ngrok http 3001
 *
 *    4. Copiar o URL público (ex: wss://abc.trycloudflare.com)
 *       e colar no campo "Relay URL" da dashboard.
 *
 *  Variáveis de ambiente:
 *    ESP32_IP        IP do ESP32 na rede local   (padrão: 192.168.1.100)
 *    ESP32_WS_PORT   Porta WebSocket do ESP32     (padrão: 81)
 *    ESP32_PATH      Path WebSocket do ESP32      (padrão: /ws)
 *    PORT            Porta local deste servidor   (padrão: 3001)
 * ═══════════════════════════════════════════════════════════
 */

const { WebSocket, WebSocketServer } = require('ws');

const PORT       = parseInt(process.env.PORT          || '3001');
const ESP32_IP   = process.env.ESP32_IP               || '192.168.1.100';
const ESP32_PORT = parseInt(process.env.ESP32_WS_PORT || '81');
const ESP32_PATH = process.env.ESP32_PATH             || '/ws';
const ESP32_URL  = `ws://${ESP32_IP}:${ESP32_PORT}${ESP32_PATH}`;

let esp32ws        = null;
let reconnectTimer = null;
let reconnectDelay = 3000;

// ─── Servidor WebSocket para browsers ──────────────────────
const wss = new WebSocketServer({ port: PORT });

function broadcast(msg) {
  const payload = typeof msg === 'string' ? msg : JSON.stringify(msg);
  wss.clients.forEach(c => {
    if (c.readyState === WebSocket.OPEN) c.send(payload);
  });
}

function broadcastRelay(connected) {
  broadcast({
    __relay: true,
    esp32_connected: connected,
    esp32_url: ESP32_URL,
    clients: wss.clients.size,
    ts: Date.now(),
  });
}

// ─── Conexão com o ESP32 ───────────────────────────────────
function connectEsp32() {
  if (esp32ws) return;
  console.log(`[ESP32] Conectando → ${ESP32_URL}`);

  try {
    esp32ws = new WebSocket(ESP32_URL, { handshakeTimeout: 5000 });

    esp32ws.on('open', () => {
      reconnectDelay = 3000;
      if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null; }
      console.log('[ESP32] ✓ Conectado');
      broadcastRelay(true);
    });

    esp32ws.on('message', data => {
      // Encaminha dados do ESP32 para todos os browsers
      broadcast(data.toString());
    });

    esp32ws.on('error', err => {
      console.error('[ESP32] Erro:', err.message);
    });

    esp32ws.on('close', () => {
      esp32ws = null;
      console.log(`[ESP32] Desconectado — nova tentativa em ${reconnectDelay / 1000}s`);
      broadcastRelay(false);
      reconnectTimer = setTimeout(() => {
        reconnectDelay = Math.min(reconnectDelay * 1.5, 30000);
        connectEsp32();
      }, reconnectDelay);
    });

  } catch (err) {
    esp32ws = null;
    console.error('[ESP32] Falha:', err.message);
    reconnectTimer = setTimeout(connectEsp32, reconnectDelay);
  }
}

// ─── Clientes browser ──────────────────────────────────────
wss.on('connection', (client, req) => {
  const addr = req.socket.remoteAddress;
  console.log(`[Browser] + ${addr}  (total: ${wss.clients.size})`);

  // Envia status atual ao conectar
  client.send(JSON.stringify({
    __relay: true,
    esp32_connected: esp32ws?.readyState === WebSocket.OPEN,
    esp32_url: ESP32_URL,
    clients: wss.clients.size,
    ts: Date.now(),
  }));

  client.on('message', data => {
    // Encaminha comandos do browser (ex: setout) para o ESP32
    if (esp32ws?.readyState === WebSocket.OPEN) {
      esp32ws.send(data.toString());
    } else {
      client.send(JSON.stringify({ __relay: true, esp32_connected: false, ts: Date.now() }));
    }
  });

  client.on('close', () => {
    console.log(`[Browser] - ${addr}  (total: ${wss.clients.size})`);
  });

  client.on('error', () => {});
});

// ─── Start ─────────────────────────────────────────────────
wss.on('listening', () => {
  console.log('\n' + '═'.repeat(50));
  console.log('  ESP32 Encoder Relay  v1.0');
  console.log('  Browser WS  → ws://localhost:' + PORT);
  console.log('  ESP32 WS    → ' + ESP32_URL);
  console.log('═'.repeat(50));
  console.log('\n  Exponha publicamente com um dos comandos:');
  console.log('    cloudflared tunnel --url http://localhost:' + PORT);
  console.log('    npx localtunnel --port ' + PORT);
  console.log('    ngrok http ' + PORT);
  console.log('\n  Depois cole o URL público no campo');
  console.log('  "Relay URL" das Configurações da dashboard.\n');
});

wss.on('error', err => {
  console.error('[Relay] Erro no servidor WS:', err.message);
  process.exit(1);
});

connectEsp32();
