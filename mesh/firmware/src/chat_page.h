// chat_page.h - the web page the board serves to a phone.
// One self-contained file: no internet needed, nothing loaded from elsewhere.
#pragma once
#include <Arduino.h>

static const char CHAT_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Mesh chat</title>
<style>
  :root { --bg:#f4f5f7; --card:#fff; --ink:#1b1f24; --dim:#667085; --line:#d9dde3; --me:#1f6feb; --them:#e7eaef; --bad:#c62828; }
  @media (prefers-color-scheme: dark) {
    :root { --bg:#0f1318; --card:#181d24; --ink:#e6e9ee; --dim:#8b95a5; --line:#2b323c; --me:#2f81f7; --them:#262d37; --bad:#ff6b6b; }
  }
  * { box-sizing: border-box; }
  body { margin:0; font:16px/1.4 system-ui, sans-serif; background:var(--bg); color:var(--ink);
         display:flex; flex-direction:column; height:100dvh; }
  header { padding:10px 16px; background:var(--card); border-bottom:1px solid var(--line); }
  header b { font-size:17px; }
  header small { display:block; color:var(--dim); font-size:12px; word-break:break-all; }
  #peers { display:flex; gap:8px; padding:10px 16px; overflow-x:auto; border-bottom:1px solid var(--line); }
  #peers button { flex:none; padding:6px 12px; border-radius:16px; border:1px solid var(--line);
                  background:var(--card); color:var(--ink); font:inherit; }
  #peers button.on { background:var(--me); border-color:var(--me); color:#fff; }
  #peers span { color:var(--dim); font-size:14px; padding:6px 0; }
  #info { padding:4px 16px; color:var(--dim); font-size:12px; min-height:22px; word-break:break-all; }
  #log { flex:1; overflow-y:auto; padding:8px 16px; display:flex; flex-direction:column; gap:6px; }
  .m { max-width:80%; padding:8px 12px; border-radius:14px; background:var(--them); align-self:flex-start;
       overflow-wrap:anywhere; white-space:pre-wrap; }
  .m.out { background:var(--me); color:#fff; align-self:flex-end; }
  .m small { display:block; font-size:11px; opacity:.75; }
  .m.failed small { color:var(--bad); opacity:1; font-weight:600; }
  form { display:flex; gap:8px; padding:10px 16px; background:var(--card); border-top:1px solid var(--line); }
  input { flex:1; min-width:0; padding:10px 12px; border-radius:10px; border:1px solid var(--line);
          background:var(--bg); color:var(--ink); font:inherit; }
  form button { padding:10px 16px; border-radius:10px; border:0; background:var(--me); color:#fff; font:inherit; }
  form button:disabled { opacity:.5; }
</style>
</head>
<body>
<header><b id="me">Mesh chat</b><small id="myfp"></small></header>
<div id="peers"><span>Looking for other devices...</span></div>
<div id="info"></div>
<div id="log"></div>
<form id="f">
  <input id="t" maxlength="200" placeholder="Pick a device above first" autocomplete="off" disabled>
  <button id="b" disabled>Send</button>
</form>
<script>
let state = { peers: [], messages: [] }, current = null;
const $ = id => document.getElementById(id);
const STATUS = ['sending...', 'delivered', 'not delivered', ''];

function draw() {
  $('me').textContent = state.name + ' (' + state.id + ')';
  $('myfp').textContent = 'my key fingerprint: ' + state.fingerprint;

  const peers = $('peers');
  peers.replaceChildren();
  if (!state.peers.length) {
    const s = document.createElement('span');
    s.textContent = 'Looking for other devices...';
    peers.append(s);
  }
  for (const p of state.peers) {
    const btn = document.createElement('button');
    btn.textContent = p.name;
    if (p.id === current) btn.className = 'on';
    btn.onclick = () => { current = p.id; draw(); };
    peers.append(btn);
  }
  const peer = state.peers.find(p => p.id === current);
  $('info').textContent = peer ? peer.name + ' key fingerprint: ' + peer.fingerprint +
                                 ' (compare in person to be sure it is really them)' : '';
  $('t').disabled = $('b').disabled = !peer;
  $('t').placeholder = peer ? 'Message to ' + peer.name : 'Pick a device above first';

  const log = $('log'), atEnd = log.scrollTop + log.clientHeight >= log.scrollHeight - 20;
  log.replaceChildren();
  for (const m of state.messages) {
    if (m.peer !== current) continue;
    const d = document.createElement('div');
    d.className = 'm' + (m.out ? ' out' : '') + (m.status === 2 ? ' failed' : '');
    d.textContent = m.text;                       // textContent: never runs as HTML
    if (m.out) { const s = document.createElement('small'); s.textContent = STATUS[m.status]; d.append(s); }
    log.append(d);
  }
  if (atEnd) log.scrollTop = log.scrollHeight;
}

async function refresh() {
  try { state = await (await fetch('/api/state')).json(); draw(); } catch (e) {}
}

$('f').onsubmit = async e => {
  e.preventDefault();
  const text = $('t').value.trim();
  if (!text || !current) return;
  $('t').value = '';
  const body = new URLSearchParams({ to: current, text });
  const r = await fetch('/api/send', { method: 'POST', body });
  if (!r.ok) $('info').textContent = 'Could not send: ' + await r.text();
  refresh();
};

refresh();
setInterval(refresh, 1500);
</script>
</body>
</html>
)HTML";
