/* ==========================================================================
   NHOM 4 - SE2044 | Dashboard Logic (Firebase Auth + Realtime Database)
   Vai tro (role) doc tu /users/<uid>/role:
     - "user"  : coi dashboard, manual/auto, relay, TAT KHAN CAP, doi WiFi
     - "admin" : tat ca + cau hinh gioi han goc quay + nguong relay
     - khac    : chi coi (viewer)
   ========================================================================== */

firebase.initializeApp(firebaseConfig);
const auth = firebase.auth();
const db   = firebase.database();

const $ = id => document.getElementById(id);
const rolePath = u => 'users/' + u.uid + '/role';

let ROLE = null;
let UNO_LINK_LIVE = false;
let ESTOP_NOW = false;

/* ---------------------------- TOAST NOTIFICATION ---------------------------- */
function toast(msg, ms = 3000) {
  const t = $('toast');
  const msgEl = $('toastMsg');
  if (msgEl) msgEl.textContent = msg;
  else t.textContent = msg;

  t.classList.add('show');
  clearTimeout(t._timer);
  t._timer = setTimeout(() => {
    t.classList.remove('show');
  }, ms);
}

/* ---------------------------- AUTHENTICATION ---------------------------- */
auth.onAuthStateChanged(async user => {
  if (!user) {
    location.replace('index.html');
    return;
  }

  // Display user info
  if ($('who')) $('who').textContent = user.email;
  if ($('userAvatar')) $('userAvatar').textContent = (user.email || 'U')[0].toUpperCase();

  try {
    const snap = await db.ref(rolePath(user)).get();
    ROLE = (snap.exists() ? snap.val() : null) || 'viewer';
  } catch (err) {
    console.warn('Khong lay duoc role:', err);
    ROLE = 'viewer';
  }

  const roleBadge = $('roleBadge');
  if (roleBadge) {
    roleBadge.textContent = ROLE.toUpperCase();
    if (ROLE === 'admin') {
      roleBadge.className = 'user-role-badge admin';
    } else {
      roleBadge.className = 'user-role-badge';
    }
  }

  if (ROLE === 'admin') {
    if ($('panelAdmin')) $('panelAdmin').style.display = 'block';
    loadAdminCfg();
  } else {
    if ($('panelAdmin')) $('panelAdmin').style.display = 'none';
  }

  if (ROLE === 'user' || ROLE === 'admin') {
    unlockControls();
  } else {
    lockControls();
    toast('Tài khoản chưa được gán quyền (user/admin). Liên hệ Admin để phân quyền.');
  }
});

function lockControls() {
  ['m-auto','m-manual','r-auto','r-on','r-off','btn-estop','btn-recover','w-save']
    .forEach(i => { if ($(i)) $(i).disabled = true; });
}

function unlockControls() {
  ['m-auto','m-manual','r-auto','r-on','r-off','btn-estop','btn-recover','w-save']
    .forEach(i => { if ($(i)) $(i).disabled = false; });
}

$('btnLogout').addEventListener('click', () => {
  firebase.auth().signOut().then(() => location.replace('index.html'));
});

/* ---------------------- TELEMETRY (/device/tele) ---------------------- */
let lastTele = null;
let lastTeleAt = 0;
let espOff = false;
const TELE_STALE_MS = 5000;   // ESP push moi 1s; > 5s khong co ban tin = ESP mat

function renderTele(d) {
  if (!d) return;

  const wifiOk   = d.wifi === 1;
  UNO_LINK_LIVE  = d.uno === 1;

  // WiFi status
  const wifiEl = $('t-wifi');
  if (wifiEl) {
    wifiEl.innerHTML = wifiOk
      ? `<span class="status-pulse ok"></span><span class="ok">Online</span> <small style="font-size:13px;color:#94a3b8">(${d.rssi ?? -50} dBm)</small>`
      : `<span class="status-pulse bad"></span><span class="bad">Mất kết nối</span>`;
  }

  // Arduino UNO status
  const unoEl = $('t-uno');
  if (unoEl) {
    unoEl.innerHTML = UNO_LINK_LIVE
      ? `<span class="status-pulse ok"></span><span class="ok">Hoạt động (UART)</span>`
      : `<span class="status-pulse bad"></span><span class="bad">Mất kết nối</span>`;
  }

  // Battery status & visual bar
  const battEl = $('t-batt');
  if (battEl) {
    if (d.pin_pct != null) {
      const v = d.pin_mv != null ? (d.pin_mv / 1000).toFixed(2) + ' V' : '';
      battEl.textContent = `${d.pin_pct}% ${v ? '(' + v + ')' : ''}`;
    } else {
      battEl.textContent = '--';
    }
  }

  const battBar = $('battBar');
  if (battBar && d.pin_pct != null) {
    const pct = Math.min(100, Math.max(0, d.pin_pct));
    battBar.style.width = pct + '%';
    if (pct > 60) {
      battBar.style.background = 'linear-gradient(90deg, #10b981, #06b6d4)';
    } else if (pct > 25) {
      battBar.style.background = 'linear-gradient(90deg, #f59e0b, #fbbf24)';
    } else {
      battBar.style.background = 'linear-gradient(90deg, #ef4444, #f87171)';
    }
  }

  // Relay status badge
  const relayEl = $('t-relay');
  if (relayEl) {
    const isCharging = !!d.relay;
    relayEl.textContent = isCharging ? '⚡ ĐANG SẠC' : 'NGẮT SẠC';
    relayEl.className = isCharging ? 'stat-badge ok' : 'stat-badge bad';
    relayEl.style.background = isCharging ? 'rgba(16, 185, 129, 0.15)' : 'rgba(239, 68, 68, 0.15)';
    relayEl.style.border = isCharging ? '1px solid rgba(16, 185, 129, 0.3)' : '1px solid rgba(239, 68, 68, 0.3)';
  }

  // Mode status badge
  const modeEl = $('t-mode');
  if (modeEl) {
    const isManual = !!d.mode;
    modeEl.textContent = isManual ? 'THỦ CÔNG' : 'AUTO BÁM TRỜI';
    modeEl.className = 'stat-badge';
    modeEl.style.background = isManual ? 'rgba(6, 182, 212, 0.15)' : 'rgba(245, 158, 11, 0.15)';
    modeEl.style.color = isManual ? '#06b6d4' : '#f59e0b';
    modeEl.style.border = isManual ? '1px solid rgba(6, 182, 212, 0.3)' : '1px solid rgba(245, 158, 11, 0.3)';
  }

  // Solar Metrics
  if ($('t-sv')) $('t-sv').textContent = d.solar_mv != null ? (d.solar_mv / 1000).toFixed(2) + ' V' : '--';
  if ($('t-si')) $('t-si').textContent = d.solar_ma != null ? d.solar_ma + ' mA' : '--';
  if ($('t-sp')) $('t-sp').textContent = d.solar_mw != null ? (d.solar_mw / 1000).toFixed(2) + ' W' : '--';
  if ($('t-en')) $('t-en').textContent = d.energy_wh != null ? d.energy_wh.toFixed(3) + ' Wh' : '--';

  // Servo Angles
  if ($('t-ang')) $('t-ang').textContent = d.pan != null ? `Pan: ${d.pan}° | Tilt: ${d.tilt}°` : '--';
  if ($('dispPanAngle') && d.pan != null) $('dispPanAngle').textContent = d.pan + '°';
  if ($('dispTiltAngle') && d.tilt != null) $('dispTiltAngle').textContent = d.tilt + '°';

  // LDR Matrix Visualizer (TL, TR, BL, BR)
  if ($('t-ldr') && d.ldr_tl != null) {
    $('t-ldr').textContent = `TL:${d.ldr_tl} | TR:${d.ldr_tr} | BL:${d.ldr_bl} | BR:${d.ldr_br}`;
  }

  const maxLdr = 1023; // Arduino ADC 10-bit range
  updateLdrCell('ldr-tl', d.ldr_tl, maxLdr);
  updateLdrCell('ldr-tr', d.ldr_tr, maxLdr);
  updateLdrCell('ldr-bl', d.ldr_bl, maxLdr);
  updateLdrCell('ldr-br', d.ldr_br, maxLdr);

  // E-Stop UI visibility
  syncEstopUi(ESTOP_NOW);
}

function updateLdrCell(prefix, val, maxRef) {
  const valEl = $(prefix + '-val');
  const pctEl = $(prefix + '-pct');
  const barEl = $(prefix + '-bar');
  if (!valEl) return;

  if (val != null) {
    valEl.textContent = val;
    const pct = Math.min(100, Math.max(0, Math.round((val / maxRef) * 100)));
    if (pctEl) pctEl.textContent = pct + '%';
    if (barEl) {
      barEl.style.width = pct + '%';
      barEl.style.background = pct > 65 ? '#f59e0b' : (pct > 30 ? '#06b6d4' : '#64748b');
    }
  } else {
    valEl.textContent = '--';
    if (pctEl) pctEl.textContent = '0%';
    if (barEl) barEl.style.width = '0%';
  }
}

function syncEstopUi(isEstop) {
  if ($('banner')) $('banner').style.display = isEstop ? 'block' : 'none';
  if ($('btn-recover')) $('btn-recover').style.display = isEstop ? 'flex' : 'none';
  if ($('btn-estop')) $('btn-estop').style.display = isEstop ? 'none' : 'flex';
}

db.ref('device/tele').on('value', s => {
  const d = s.val();
  if (!d) return;
  lastTele = d;
  lastTeleAt = Date.now();
  espOff = false;
  renderTele(d);
});

// Stale watchdog
setInterval(() => {
  if (!lastTeleAt) return;
  const off = Date.now() - lastTeleAt > TELE_STALE_MS;
  if (off === espOff) return;
  espOff = off;
  if (off) {
    if ($('t-wifi')) $('t-wifi').innerHTML = '<span class="status-pulse bad"></span><span class="bad">ESP Mất kết nối</span>';
    if ($('t-uno')) $('t-uno').innerHTML = '<span class="status-pulse bad"></span><span class="bad">Mất kết nối</span>';
    if ($('t-batt')) $('t-batt').textContent = '--';
  } else {
    renderTele(lastTele);
  }
}, 2000);

/* ---------------------- COMMAND CONTROLS (/cmd) ---------------------- */

// Sync emergency state
db.ref('cmd/emergency').on('value', s => {
  ESTOP_NOW = s.val() === true;
  syncEstopUi(ESTOP_NOW);
});

// Sync tracking mode
db.ref('cmd/mode').on('value', s => {
  const m = s.val() || 'auto';
  $('m-auto').classList.toggle('on', m === 'auto');
  $('m-manual').classList.toggle('on', m === 'manual');
  if ($('manualBox')) $('manualBox').style.display = m === 'manual' ? 'block' : 'none';
});

// Sync relay commands
db.ref('cmd/relay').on('value', s => {
  const r = s.val() || 'auto';
  $('r-auto').classList.toggle('on', r === 'auto');
  $('r-on').classList.toggle('on',   r === 'on');
  $('r-off').classList.toggle('on',  r === 'off');
});

// Sync servo angle sliders initially from cmd
db.ref('cmd/pan').on('value', s => {
  if (s.exists() && document.activeElement !== $('sl-pan')) {
    const val = s.val();
    if ($('sl-pan')) $('sl-pan').value = val;
    if ($('v-pan')) $('v-pan').textContent = val;
  }
});

db.ref('cmd/tilt').on('value', s => {
  if (s.exists() && document.activeElement !== $('sl-tilt')) {
    const val = s.val();
    if ($('sl-tilt')) $('sl-tilt').value = val;
    if ($('v-tilt')) $('v-tilt').textContent = val;
  }
});

$('m-auto').addEventListener('click', () => {
  db.ref('cmd/mode').set('auto');
  toast('Đã chuyển sang chế độ AUTO (Tự động bám trời)');
});

$('m-manual').addEventListener('click', () => {
  db.ref('cmd/mode').set('manual');
  toast('Đã kích hoạt chế độ THỦ CÔNG (Manual control)');
});

// Debounced slider handlers
let panDeb = null, tiltDeb = null;

if ($('sl-pan')) {
  $('sl-pan').addEventListener('input', e => {
    $('v-pan').textContent = e.target.value;
    clearTimeout(panDeb);
    panDeb = setTimeout(() => {
      db.ref('cmd/pan').set(parseInt(e.target.value, 10));
    }, 200);
  });
}

if ($('sl-tilt')) {
  $('sl-tilt').addEventListener('input', e => {
    $('v-tilt').textContent = e.target.value;
    clearTimeout(tiltDeb);
    tiltDeb = setTimeout(() => {
      db.ref('cmd/tilt').set(parseInt(e.target.value, 10));
    }, 200);
  });
}

// Global preset helper for quick angles
window.setSlider = function(sliderId, labelId, val, cmdPath) {
  if ($(sliderId)) $(sliderId).value = val;
  if ($(labelId)) $(labelId).textContent = val;
  db.ref(cmdPath).set(parseInt(val, 10));
  toast(`Đặt góc ${sliderId === 'sl-pan' ? 'Ngang (Pan)' : 'Dọc (Tilt)'} = ${val}°`);
};

// Relay buttons
$('r-auto').addEventListener('click', () => {
  db.ref('cmd/relay').set('auto');
  toast('Relay chuyển về chế độ TỰ ĐỘNG');
});

$('r-on').addEventListener('click', () => {
  db.ref('cmd/relay').set('on');
  toast('Relay đã BẬT (Đang sạc pin)');
});

$('r-off').addEventListener('click', () => {
  db.ref('cmd/relay').set('off');
  toast('Relay đã TẮT (Ngắt sạc)');
});

// Emergency Stop
function triggerEstop() {
  if (!confirm('XÁC NHẬN TẮT KHẨN CẤP?\n\nRelay sẽ NGẮT NGAY LẬP TỨC (ngắt sạc pin) để bảo vệ mạch và pin,\nvà không tự động đóng lại cho đến khi nhấn KHÔI PHỤC.')) {
    return;
  }
  db.ref('cmd/relay').set('off');
  db.ref('cmd/emergency').set(true);
  toast('⚠️ ĐÃ TẮT KHẨN CẤP - Relay đã ngắt sạc!');
}

function recoverEstop() {
  if (!confirm('XÁC NHẬN KHÔI PHỤC HỆ THỐNG?\n\nRelay sẽ trở về chế độ TỰ ĐỘNG hoạt động bình thường.')) {
    return;
  }
  db.ref('cmd/emergency').set(false);
  db.ref('cmd/relay').set('auto');
  toast('✅ Đã khôi phục an toàn - Relay trở về TỰ ĐỘNG');
}

$('btn-estop').addEventListener('click', triggerEstop);
$('btn-recover').addEventListener('click', recoverEstop);
if ($('btn-banner-recover')) {
  $('btn-banner-recover').addEventListener('click', recoverEstop);
}

/* ---------------------- ADMIN CONFIGURATION ---------------------- */
function loadAdminCfg() {
  db.ref('cmd').get().then(s => {
    const v = s.val() || {};
    if ($('c-panMin'))    $('c-panMin').value    = v.cfgPanMin    ?? 0;
    if ($('c-panMax'))    $('c-panMax').value    = v.cfgPanMax    ?? 150;
    if ($('c-tiltMin'))   $('c-tiltMin').value   = v.cfgTiltMin   ?? 0;
    if ($('c-tiltMax'))   $('c-tiltMax').value   = v.cfgTiltMax   ?? 160;
    if ($('c-solarOn'))   $('c-solarOn').value   = v.cfgSolarOn   ?? 3700;
    if ($('c-solarOff'))  $('c-solarOff').value  = v.cfgSolarOff  ?? 3300;
    if ($('c-battStop'))  $('c-battStop').value  = v.cfgBattStop  ?? 100;
    if ($('c-battResume'))$('c-battResume').value= v.cfgBattResume?? 90;
  });
}

if ($('c-save')) {
  $('c-save').addEventListener('click', async () => {
    const pm = parseInt($('c-panMin').value, 10);
    const px = parseInt($('c-panMax').value, 10);
    const tm = parseInt($('c-tiltMin').value, 10);
    const tx = parseInt($('c-tiltMax').value, 10);

    if (isNaN(pm) || isNaN(px) || pm >= px || pm < 0 || px > 179 || isNaN(tm) || isNaN(tx) || tm >= tx || tm < 0 || tx > 179) {
      alert('Giới hạn góc không hợp lệ! Vui lòng đảm bảo (Min < Max) và nằm trong khoảng từ 0 đến 179 độ.');
      return;
    }

    const so = parseInt($('c-solarOn').value, 10);
    const sf = parseInt($('c-solarOff').value, 10);
    if (isNaN(so) || isNaN(sf) || so < sf) {
      alert('Ngưỡng điện áp Solar ON phải lớn hơn hoặc bằng Solar OFF.');
      return;
    }

    const bs = parseInt($('c-battStop').value, 10);
    const br = parseInt($('c-battResume').value, 10);
    if (isNaN(bs) || isNaN(br) || bs < br) {
      alert('Ngưỡng Pin STOP phải lớn hơn hoặc bằng Pin RESUME.');
      return;
    }

    const upd = {
      cfgPanMin: pm, cfgPanMax: px, cfgTiltMin: tm, cfgTiltMax: tx,
      cfgSolarOn: so, cfgSolarOff: sf, cfgBattStop: bs, cfgBattResume: br,
    };

    try {
      await db.ref('cmd').update(upd);
      toast('Đã lưu cấu hình Admin thành công! ESP sẽ gửi lệnh tới UNO.');
    } catch (e) {
      alert('Lỗi lưu cấu hình: ' + e.message);
    }
  });
}

/* ---------------------- WIFI SETTINGS ---------------------- */
if ($('w-save')) {
  $('w-save').addEventListener('click', async () => {
    const ssid = $('w-ssid').value.trim();
    const pass = $('w-pass').value;

    if (!ssid) {
      alert('Vui lòng nhập tên WiFi (SSID).');
      return;
    }

    if (!confirm(`Xác nhận đổi WiFi của ESP32 thành "${ssid}"?\n\nThiết bị sẽ KHỞI ĐỘNG LẠI để kết nối vào mạng WiFi mới.\nNếu không kết nối được, ESP32 sẽ mở cổng cấu hình Access Point.`)) {
      return;
    }

    try {
      const tick = Math.floor(Date.now() / 1000);
      await db.ref('cmd').update({ wifiSsid: ssid, wifiPass: pass, wifiTick: tick });
      toast('Đã lưu cấu hình WiFi. ESP32 sẽ khởi động lại...');
      $('w-ssid').value = '';
      $('w-pass').value = '';
    } catch (e) {
      alert('Lỗi khi lưu WiFi: ' + e.message);
    }
  });
}

/* ---------------------- HISTORY ANALYTICS & CANVAS CHART ---------------------- */
if ($('h-load')) {
  $('h-load').addEventListener('click', async () => {
    const btn = $('h-load');
    btn.disabled = true;
    btn.innerHTML = 'Đang tải dữ liệu...';
    try {
      const s = await db.ref('history').limitToLast(96).get();
      const canvas = $('his');
      if (!s.exists()) {
        drawHistoryChart(canvas, []);
        toast('Chưa có bản ghi lịch sử nào.');
      } else {
        const vals = Object.values(s.val())
          .map(x => x.energy_wh ?? null)
          .filter(v => v != null);
        drawHistoryChart(canvas, vals);
        toast(`Đã tải thành công ${vals.length} bản ghi năng lượng!`);
      }
    } catch (err) {
      toast('Lỗi khi tải lịch sử: ' + err.message);
    } finally {
      btn.disabled = false;
      btn.innerHTML = `<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="1 4 1 10 7 10"></polyline><path d="M3.51 15a9 9 0 1 0 2.13-9.36L1 10"></path></svg> <span>Tải dữ liệu mới nhất</span>`;
    }
  });
}

function drawHistoryChart(canvas, vals) {
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  const dpr = window.devicePixelRatio || 1;
  const rect = canvas.getBoundingClientRect();
  const W = rect.width || canvas.clientWidth || 400;
  const H = rect.height || canvas.clientHeight || 180;

  canvas.width = W * dpr;
  canvas.height = H * dpr;
  ctx.scale(dpr, dpr);

  ctx.clearRect(0, 0, W, H);

  // Background grid
  ctx.strokeStyle = 'rgba(255, 255, 255, 0.05)';
  ctx.lineWidth = 1;
  const gridLines = 4;
  for (let i = 0; i <= gridLines; i++) {
    const y = (H - 24) * (i / gridLines) + 12;
    ctx.beginPath();
    ctx.moveTo(10, y);
    ctx.lineTo(W - 10, y);
    ctx.stroke();
  }

  if (!vals || vals.length === 0) {
    ctx.fillStyle = '#64748b';
    ctx.font = '13px "Plus Jakarta Sans", sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText('Chưa có dữ liệu lịch sử năng lượng để hiển thị', W / 2, H / 2);
    return;
  }

  const maxV = Math.max(...vals, 0.005);
  const minV = 0;
  const paddingX = 14;
  const paddingTop = 20;
  const paddingBottom = 20;
  const plotW = W - paddingX * 2;
  const plotH = H - paddingTop - paddingBottom;
  const step = vals.length > 1 ? plotW / (vals.length - 1) : plotW;

  // Build points
  const points = vals.map((v, i) => {
    const x = paddingX + i * step;
    const y = paddingTop + plotH - ((v - minV) / (maxV - minV)) * plotH;
    return { x, y, v };
  });

  // Gradient area fill
  const grad = ctx.createLinearGradient(0, paddingTop, 0, H - paddingBottom);
  grad.addColorStop(0, 'rgba(6, 182, 212, 0.35)');
  grad.addColorStop(1, 'rgba(6, 182, 212, 0.0)');

  ctx.beginPath();
  ctx.moveTo(points[0].x, H - paddingBottom);
  points.forEach(p => ctx.lineTo(p.x, p.y));
  ctx.lineTo(points[points.length - 1].x, H - paddingBottom);
  ctx.closePath();
  ctx.fillStyle = grad;
  ctx.fill();

  // Line stroke
  ctx.beginPath();
  ctx.strokeStyle = '#06b6d4';
  ctx.lineWidth = 2.5;
  ctx.lineJoin = 'round';
  points.forEach((p, i) => {
    i === 0 ? ctx.moveTo(p.x, p.y) : ctx.lineTo(p.x, p.y);
  });
  ctx.stroke();

  // Dots on data points if not too dense
  if (points.length <= 48) {
    points.forEach(p => {
      ctx.beginPath();
      ctx.arc(p.x, p.y, 3, 0, Math.PI * 2);
      ctx.fillStyle = '#06b6d4';
      ctx.fill();
      ctx.lineWidth = 1.5;
      ctx.strokeStyle = '#090d16';
      ctx.stroke();
    });
  }

  // Label highest value
  ctx.fillStyle = '#94a3b8';
  ctx.font = '11px "JetBrains Mono", monospace';
  ctx.textAlign = 'left';
  ctx.fillText(`Max: ${maxV.toFixed(3)} Wh`, paddingX, 14);
}

// Draw blank chart initially
window.addEventListener('load', () => {
  const canvas = $('his');
  if (canvas) drawHistoryChart(canvas, []);
});
