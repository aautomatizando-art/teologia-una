/* ── Estado ──────────────────────────────────────── */
let muted         = false;
let overlayActive = false;
let reconnectTimer;
let audioCtx;
let historyCount  = 0;

/* ── DOM refs ────────────────────────────────────── */
const connDot        = document.getElementById('connDot');
const connLabel      = document.getElementById('connLabel');
const statAlarms     = document.getElementById('statAlarms');
const statFaults     = document.getElementById('statFaults');
const statHistory    = document.getElementById('statHistory');
const statLast       = document.getElementById('statLast');
const alarmList      = document.getElementById('alarmList');
const emptyAlarms    = document.getElementById('emptyAlarms');
const historyBody    = document.getElementById('historyBody');
const alarmOverlay   = document.getElementById('alarmOverlay');
const overlayAddr    = document.getElementById('overlayAddr');
const overlayDesc    = document.getElementById('overlayDesc');
const btnMute        = document.getElementById('btnMute');
const btnClear       = document.getElementById('btnClear');
const btnDismiss     = document.getElementById('btnDismissOverlay');
const muteX          = document.getElementById('muteX');
const soundWave      = document.getElementById('soundWave');
const clockEl        = document.getElementById('clock');

/* ── Clock ───────────────────────────────────────── */
function tickClock() {
  clockEl.textContent = new Date().toLocaleTimeString('pt-BR');
}
tickClock();
setInterval(tickClock, 1000);

/* ── Audio (Web Audio API – sem arquivos externos) ── */
function getAudioCtx() {
  if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
  return audioCtx;
}

function beep(freq = 880, duration = 0.15, vol = 0.4) {
  if (muted) return;
  try {
    const ctx  = getAudioCtx();
    const osc  = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.connect(gain);
    gain.connect(ctx.destination);
    osc.type            = 'square';
    osc.frequency.value = freq;
    gain.gain.setValueAtTime(vol, ctx.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + duration);
    osc.start(ctx.currentTime);
    osc.stop(ctx.currentTime + duration);
  } catch (_) {}
}

function alarmSound() {
  if (muted) return;
  beep(1047, 0.12, 0.5);
  setTimeout(() => beep(880, 0.12, 0.5), 160);
  setTimeout(() => beep(1047, 0.12, 0.5), 320);
}

/* ── Mute button ─────────────────────────────────── */
btnMute.addEventListener('click', () => {
  muted = !muted;
  btnMute.classList.toggle('muted', muted);
  muteX.style.display     = muted ? '' : 'none';
  soundWave.style.display = muted ? 'none' : '';
});

/* ── Clear history ───────────────────────────────── */
btnClear.addEventListener('click', () => {
  historyBody.innerHTML = '<tr><td colspan="4" class="empty-row">Histórico limpo.</td></tr>';
  historyCount = 0;
  statHistory.textContent = '0';
});

/* ── Overlay ─────────────────────────────────────── */
btnDismiss.addEventListener('click', () => {
  alarmOverlay.classList.remove('visible');
  overlayActive = false;
});

function showOverlay(event) {
  overlayAddr.textContent = `Endereço: ${event.address}`;
  overlayDesc.textContent = event.description;
  alarmOverlay.classList.add('visible');
  overlayActive = true;
  alarmSound();
}

/* ── Render active alarms ────────────────────────── */
function renderAlarms(activeAlarms) {
  alarmList.innerHTML = '';

  if (!activeAlarms || activeAlarms.length === 0) {
    alarmList.appendChild(emptyAlarms);
    return;
  }

  // Alarmes primeiro, depois falhas
  const sorted = [...activeAlarms].sort((a, b) => {
    const order = { ALARME: 0, FALHA: 1, SUPERVISAO: 2 };
    return (order[a.type] ?? 9) - (order[b.type] ?? 9);
  });

  sorted.forEach(ev => {
    const card = document.createElement('div');
    card.className = `alarm-card ${ev.type}`;
    card.innerHTML = `
      <div class="alarm-card-top">
        <span class="alarm-badge badge-${ev.type}">${ev.type}</span>
        <span class="alarm-time">${ev.date} ${ev.time}</span>
      </div>
      <div class="alarm-addr">End: ${String(ev.address).padStart(3, '0')}</div>
      <div class="alarm-desc">${ev.description}</div>
    `;
    alarmList.appendChild(card);
  });
}

/* ── Render stats ────────────────────────────────── */
function renderStats(stats) {
  statAlarms.textContent = stats.alarms ?? 0;
  statFaults.textContent = stats.faults ?? 0;
}

/* ── Add row to history table ────────────────────── */
function prependHistory(event) {
  // Remove "aguardando" row
  const emptyRow = historyBody.querySelector('.empty-row');
  if (emptyRow) emptyRow.parentElement.remove();

  // Limit to 200 rows in DOM
  while (historyBody.rows.length >= 200) {
    historyBody.deleteRow(historyBody.rows.length - 1);
  }

  const tr = document.createElement('tr');
  tr.className = 'new-row';
  tr.innerHTML = `
    <td class="time-cell">${event.date} ${event.time}</td>
    <td><span class="type-badge type-${event.type}">${event.type}</span></td>
    <td class="addr-cell">${String(event.address).padStart(3, '0')}</td>
    <td>${event.description}</td>
  `;
  historyBody.insertBefore(tr, historyBody.firstChild);

  historyCount++;
  statHistory.textContent = historyCount;
  statLast.textContent    = event.time;
}

/* ── Populate history (init) ─────────────────────── */
function populateHistory(events) {
  if (!events || events.length === 0) return;
  historyBody.innerHTML = '';
  events.forEach(ev => {
    const tr = document.createElement('tr');
    tr.innerHTML = `
      <td class="time-cell">${ev.date} ${ev.time}</td>
      <td><span class="type-badge type-${ev.type}">${ev.type}</span></td>
      <td class="addr-cell">${String(ev.address).padStart(3, '0')}</td>
      <td>${ev.description}</td>
    `;
    historyBody.appendChild(tr);
  });
  historyCount          = events.length;
  statHistory.textContent = historyCount;
  if (events[0]) statLast.textContent = events[0].time;
}

/* ── WebSocket ───────────────────────────────────── */
function connect() {
  const proto = location.protocol === 'https:' ? 'wss' : 'ws';
  const ws    = new WebSocket(`${proto}://${location.host}`);

  ws.addEventListener('open', () => {
    connDot.className   = 'conn-dot connected';
    connLabel.textContent = 'Conectado';
    clearTimeout(reconnectTimer);
  });

  ws.addEventListener('close', () => {
    connDot.className    = 'conn-dot disconnected';
    connLabel.textContent = 'Desconectado';
    reconnectTimer = setTimeout(connect, 3000);
  });

  ws.addEventListener('error', () => ws.close());

  ws.addEventListener('message', ({ data }) => {
    let msg;
    try { msg = JSON.parse(data); } catch { return; }

    if (msg.type === 'init') {
      renderAlarms(msg.activeAlarms);
      renderStats(msg.stats);
      populateHistory(msg.history);
    }

    if (msg.type === 'event') {
      renderAlarms(msg.activeAlarms);
      renderStats(msg.stats);
      prependHistory(msg.event);

      // Overlay + beep apenas para novos alarmes
      if (msg.event.type === 'ALARME') {
        if (!overlayActive) showOverlay(msg.event);
        else alarmSound();
      } else if (msg.event.type === 'FALHA') {
        beep(440, 0.2, 0.3);
      }
    }
  });
}

connect();
