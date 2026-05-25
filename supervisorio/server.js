const express    = require('express');
const http       = require('http');
const path       = require('path');
const { WebSocketServer } = require('ws');

const PORT        = process.env.PORT        || 3000;
const SERIAL_PORT = process.env.SERIAL_PORT || '/dev/ttyUSB0';
const BAUD_RATE   = parseInt(process.env.BAUD_RATE || '115200');
const SIMULATE    = process.env.SIMULATE === 'true';

const app    = express();
const server = http.createServer(app);
const wss    = new WebSocketServer({ server });

app.use(express.static(path.join(__dirname, 'public')));

// Estado em memória
const activeAlarms = new Map();   // key = endereço (string)
const eventHistory = [];
const MAX_HISTORY  = 300;

// ──────────────────────────────────────────────
// Parser de linha da central / Arduino
// Formato esperado:
//   "DD/MM/AAAA HH:MM:SS  ALARME  END:042  BOTOEIRA PAV 3"
// ──────────────────────────────────────────────
function parseLine(raw) {
    const line = raw.trim();
    if (!line) return null;

    const endMatch = line.match(/END:(\d+)/i);
    if (!endMatch) return null;

    const address = parseInt(endMatch[1], 10);

    let type = 'EVENTO';
    if (/ALARME/i.test(line))  type = 'ALARME';
    else if (/FALHA/i.test(line))   type = 'FALHA';
    else if (/NORMAL/i.test(line))  type = 'NORMAL';
    else if (/SUPERV/i.test(line))  type = 'SUPERVISAO';
    else if (/RETORNO/i.test(line)) type = 'NORMAL';

    const descMatch = line.match(/END:\d+\s+(.+)/i);
    const description = descMatch ? descMatch[1].trim() : `ENDEREÇO ${address}`;

    const timeMatch = line.match(/(\d{2}:\d{2}:\d{2})/);
    const dateMatch = line.match(/(\d{2}\/\d{2}\/\d{4})/);
    const now = new Date();
    const time = timeMatch ? timeMatch[1] : now.toLocaleTimeString('pt-BR');
    const date = dateMatch ? dateMatch[0] : now.toLocaleDateString('pt-BR');

    return { address, type, description, time, date, raw: line, ts: Date.now() };
}

function calcStats() {
    const vals = Array.from(activeAlarms.values());
    return {
        alarms: vals.filter(e => e.type === 'ALARME').length,
        faults: vals.filter(e => e.type === 'FALHA').length,
        total:  vals.length,
    };
}

function broadcast(msg) {
    const payload = JSON.stringify(msg);
    wss.clients.forEach(ws => {
        if (ws.readyState === 1) ws.send(payload);
    });
}

function processEvent(event) {
    const key = String(event.address);

    if (event.type === 'ALARME' || event.type === 'FALHA' || event.type === 'SUPERVISAO') {
        activeAlarms.set(key, event);
    } else if (event.type === 'NORMAL') {
        activeAlarms.delete(key);
    }

    eventHistory.unshift(event);
    if (eventHistory.length > MAX_HISTORY) eventHistory.pop();

    broadcast({
        type:         'event',
        event,
        activeAlarms: Array.from(activeAlarms.values()),
        stats:        calcStats(),
    });
}

// ──────────────────────────────────────────────
// WebSocket – envia estado inicial ao conectar
// ──────────────────────────────────────────────
wss.on('connection', ws => {
    ws.send(JSON.stringify({
        type:         'init',
        activeAlarms: Array.from(activeAlarms.values()),
        history:      eventHistory.slice(0, 100),
        stats:        calcStats(),
    }));
});

// ──────────────────────────────────────────────
// Modo simulação (npm run simulate)
// ──────────────────────────────────────────────
const DEMO_DEVICES = [
    'BOTOEIRA PAV 1',    'BOTOEIRA PAV 2',    'BOTOEIRA PAV 3',
    'DETECTOR SALA 101', 'DETECTOR CORREDOR', 'DETECTOR COPA',
    'SIRENE EXTERNAS',   'MODULO SAIDA 1',    'BOTOEIRA RECEPCAO',
    'DETECTOR GARAGEM',  'BOTOEIRA ESCADA A', 'DETECTOR LAB',
];

if (SIMULATE) {
    console.log('[SIMULAÇÃO] Gerando eventos a cada 3 s…');
    let counter = 0;
    setInterval(() => {
        counter++;
        const addr    = Math.floor(Math.random() * 20) + 1;
        const types   = ['ALARME', 'ALARME', 'FALHA', 'NORMAL'];
        const tipo    = types[Math.floor(Math.random() * types.length)];
        const desc    = DEMO_DEVICES[Math.floor(Math.random() * DEMO_DEVICES.length)];
        const now     = new Date();
        const line    = `${now.toLocaleDateString('pt-BR')} ${now.toLocaleTimeString('pt-BR')}  ${tipo}  END:${String(addr).padStart(3, '0')}  ${desc}`;
        console.log('[SIM]', line);
        const event = parseLine(line);
        if (event) processEvent(event);
    }, 3000);
}

// ──────────────────────────────────────────────
// Porta serial real (Arduino via MAX232)
// ──────────────────────────────────────────────
if (!SIMULATE) {
    try {
        const { SerialPort }       = require('serialport');
        const { ReadlineParser }   = require('@serialport/parser-readline');

        const serial = new SerialPort({ path: SERIAL_PORT, baudRate: BAUD_RATE });
        const parser = serial.pipe(new ReadlineParser({ delimiter: '\n' }));

        serial.on('open',  ()    => console.log(`[SERIAL] Aberta: ${SERIAL_PORT} @ ${BAUD_RATE} bps`));
        serial.on('error', err  => console.error('[SERIAL] Erro:', err.message));

        parser.on('data', raw => {
            const line = raw.trim();
            console.log('[RX]', line);
            const event = parseLine(line);
            if (event) processEvent(event);
        });
    } catch (err) {
        console.error('[SERIAL] Falha ao abrir porta:', err.message);
        console.log('  → Use: SIMULATE=true npm start   para testar sem hardware');
    }
}

server.listen(PORT, () => {
    console.log(`\nSistema supervisório disponível em http://localhost:${PORT}`);
    console.log(`Modo: ${SIMULATE ? 'SIMULAÇÃO' : `Serial ${SERIAL_PORT}`}\n`);
});
