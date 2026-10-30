/**
 * SolarController IoT Dashboard
 * Chỉ hiển thị dữ liệu thực từ ESP32 qua Firebase RTDB.
 * Telemetry schema: angle, mode, voltage, soc, ldr_tl, ldr_tr, ldr_bl, ldr_br, diff
 */

// ==============================================================================
// 1. CẤU HÌNH
// ==============================================================================
const DEFAULT_CONFIG = {
    firebaseUrl: 'https://solarcontroller-1cc93-default-rtdb.asia-southeast1.firebasedatabase.app'
};

let config = loadConfig();
let firebaseApp = null;
let firebaseDb = null;
let hasTelemetry = false;
let lastTelemetryAt = 0;
let staleRendered = false;

const STALE_MS = 10000;

function renderStaleValues() {
    if (staleRendered) return;
    staleRendered = true;

    document.getElementById('valCurAngle').innerText = '—';
    const tagMode = document.getElementById('tagMode');
    tagMode.innerText = 'ESP32 NGẮT KẾT NỐI';
    tagMode.style.borderColor = 'rgba(255,255,255,0.2)';
    tagMode.style.color = '#94a3b8';

    document.getElementById('valSOC').innerText = '—';
    document.getElementById('valVoltage').innerText = '— V';
    document.getElementById('batteryFluid').style.height = '0%';

    ['valLdrTL', 'valLdrTR', 'valLdrBL', 'valLdrBR'].forEach(id => {
        document.getElementById(id).innerText = '—';
    });
    ['fillTL', 'fillTR', 'fillBL', 'fillBR'].forEach(id => {
        document.getElementById(id).style.width = '0%';
    });

    document.getElementById('valDiff').innerText = '—';
    document.getElementById('diffNeedle').style.left = '50%';

    const navWifi = document.getElementById('navWifi');
    if (navWifi) navWifi.innerText = 'WiFi: —';
}

function updateEspStatusChip() {
    const dot = document.getElementById('espDot');
    const label = document.getElementById('espLabel');
    if (!dot || !label) return;
    const age = lastTelemetryAt ? Date.now() - lastTelemetryAt : Infinity;
    if (age < STALE_MS) {
        dot.className = 'dot dot-emerald';
        label.innerText = 'ESP32: Online';
    } else {
        dot.className = 'dot dot-red';
        label.innerText = lastTelemetryAt ? 'ESP32: Mất dữ liệu' : 'ESP32: Chưa kết nối';
        renderStaleValues();
    }
}
setInterval(updateEspStatusChip, 2000);

function loadConfig() {
    const saved = localStorage.getItem('solar_controller_cfg');
    if (saved) {
        try {
            const parsed = JSON.parse(saved);
            if (parsed.firebaseUrl && !parsed.firebaseUrl.includes('your-project')) {
                return { ...DEFAULT_CONFIG, firebaseUrl: parsed.firebaseUrl };
            }
        } catch (e) {}
    }
    return { ...DEFAULT_CONFIG };
}

function saveConfig(newConfig) {
    config = { ...config, ...newConfig };
    localStorage.setItem('solar_controller_cfg', JSON.stringify(config));
}

// ==============================================================================
// 2. KẾT NỐI FIREBASE
// ==============================================================================
function boot() {
    init3DVisualizer();
    setupUIEventListeners();
    applyConfigToModal();
    startConnection();
}

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', boot);
} else {
    boot();
}

function startConnection() {
    const netLabel = document.getElementById('netLabel');
    const chipNet = document.getElementById('chipNet');
    netLabel.innerText = 'Đang đồng bộ...';
    chipNet.querySelector('.dot').className = 'dot dot-gray';

    if (!config.firebaseUrl) {
        openConfigModal();
        return;
    }

    try {
        if (!firebase.apps.length) {
            firebaseApp = firebase.initializeApp({ databaseURL: config.firebaseUrl });
        }
        firebaseDb = firebase.database();

        firebaseDb.ref('.info/connected').on('value', snap => {
            if (snap.val()) {
                netLabel.innerText = hasTelemetry ? 'Cloud Online' : 'Cloud Online — chưa có dữ liệu';
                chipNet.querySelector('.dot').className = 'dot dot-emerald';
            } else {
                netLabel.innerText = 'Mất kết nối Cloud';
                chipNet.querySelector('.dot').className = 'dot dot-gray';
            }
        });

        firebaseDb.ref('/solar_controller/telemetry').on('value', snap => {
            const data = snap.val();
            if (data) {
                onTelemetryReceived(data);
            } else {
                // DB null -> không có dữ liệu thật -> hiện trạng thái ngắt
                lastTelemetryAt = 0;
                staleRendered = false;
                updateEspStatusChip();
            }
        });
    } catch (err) {
        netLabel.innerText = 'Lỗi kết nối Cloud';
        chipNet.querySelector('.dot').className = 'dot dot-gray';
    }
}

// ==============================================================================
// 3. RENDER TELEMETRY (CHỈ DỮ LIỆU THẬT)
// ==============================================================================
function onTelemetryReceived(data) {
    hasTelemetry = true;
    lastTelemetryAt = Date.now();
    staleRendered = false;
    updateEspStatusChip();

    const netLabel = document.getElementById('netLabel');
    if (netLabel.innerText.includes('chưa có dữ liệu')) {
        netLabel.innerText = 'Cloud Online';
    }

    const has = v => v !== undefined && v !== null;

    handleMotionTelemetry(data);

    // Cấu hình chuyển động echo từ ESP -> điền bảng (trừ khi đang gõ/đang đo)
    if (!measuring && !pendingStop && Date.now() > cfgLockUntil) {
        const ae = document.activeElement;
        if (!ae || !ae.closest || !ae.closest('#motionSection')) applyMotionConfig(data);
    }

    // 1. Servo continuous: trạng thái chạy + hướng đích (không có góc thật)
    const target = data.move_target || '-';
    const isMoving = data.moving === 1 || data.moving === true;
    document.getElementById('valCurAngle').innerText =
        isMoving ? `▶ ${target}` : (target === '-' ? '■ DỪNG' : `■ ${target}`);
    update3DTracker(targetToAngle(target), data);

    const tagMode = document.getElementById('tagMode');
    if (has(data.mode)) {
        const isAuto = data.mode === 'AUTO';
        tagMode.innerText = isAuto ? 'AUTO TRACKING' : 'MANUAL CONTROL';
        tagMode.style.borderColor = isAuto ? 'var(--primary)' : 'var(--accent)';
        tagMode.style.color = isAuto ? 'var(--primary-bright)' : 'var(--accent)';

        const btnAuto = document.getElementById('btnAutoMode');
        const btnManual = document.getElementById('btnManualMode');
        if (isAuto) {
            btnAuto.classList.add('active');
            btnManual.classList.remove('active');
        } else {
            btnAuto.classList.remove('active');
            btnManual.classList.add('active');
        }
    } else {
        tagMode.innerText = 'CHƯA CÓ SENSOR';
        tagMode.style.borderColor = 'rgba(255,255,255,0.2)';
        tagMode.style.color = '#94a3b8';
    }

    // 2. Pin: soc + voltage
    const fluid = document.getElementById('batteryFluid');
    if (has(data.soc) && has(data.voltage)) {
        const soc = Math.max(0, Math.min(100, Math.round(data.soc)));
        document.getElementById('valSOC').innerText = soc;
        document.getElementById('valVoltage').innerText = Number(data.voltage).toFixed(2) + ' V';
        fluid.style.height = soc + '%';
        if (soc > 40) {
            fluid.style.background = 'linear-gradient(180deg, #10b981 0%, #059669 100%)';
        } else if (soc > 20) {
            fluid.style.background = 'linear-gradient(180deg, #f59e0b 0%, #d97706 100%)';
        } else {
            fluid.style.background = 'linear-gradient(180deg, #ef4444 0%, #b91c1c 100%)';
        }
    } else {
        document.getElementById('valSOC').innerText = '—';
        document.getElementById('valVoltage').innerText = '— V';
        fluid.style.height = '0%';
    }

    // 3. LDR (4 ADC) + diff
    const maxADC = 4095;
    const ldrCells = [
        ['valLdrTL', 'fillTL', data.ldr_tl],
        ['valLdrTR', 'fillTR', data.ldr_tr],
        ['valLdrBL', 'fillBL', data.ldr_bl],
        ['valLdrBR', 'fillBR', data.ldr_br],
    ];
    ldrCells.forEach(([valId, fillId, v]) => {
        document.getElementById(valId).innerText = has(v) ? v : '—';
        document.getElementById(fillId).style.width = has(v) ? ((v / maxADC) * 100) + '%' : '0%';
    });

    if (has(data.diff)) {
        const clampedDiff = Math.max(-2000, Math.min(2000, data.diff));
        document.getElementById('diffNeedle').style.left = (((clampedDiff + 2000) / 4000) * 100) + '%';
        document.getElementById('valDiff').innerText = (data.diff > 0 ? '+' : '') + data.diff;
    } else {
        document.getElementById('diffNeedle').style.left = '50%';
        document.getElementById('valDiff').innerText = '—';
    }

    // 4. Thông tin WiFi thực từ firmware (nếu ESP chạy bản mới)
    const navWifi = document.getElementById('navWifi');
    if (navWifi && data.wifi_ip) {
        navWifi.innerText = `WiFi: ${data.wifi_ssid || '—'} • ${data.wifi_ip}`;
    }
}

// ==============================================================================
// 4. GỬI LỆNH (GHI /solar_controller/command)
// ==============================================================================
function sendCommand(cmdPayload) {
    const syncStatus = document.getElementById('cmdSyncStatus');
    syncStatus.innerText = 'Đang gửi...';
    syncStatus.style.color = 'var(--primary-bright)';

    // Thêm timestamp để firmware biết command mới hay cũ
    cmdPayload.timestamp = Date.now();

    if (firebaseDb) {
        firebaseDb.ref('/solar_controller/command').set(cmdPayload)
            .then(() => {
                syncStatus.innerText = 'Đã đồng bộ Cloud';
                syncStatus.style.color = 'var(--success)';
            })
            .catch(() => {
                syncStatus.innerText = 'Lỗi gửi Cloud';
                syncStatus.style.color = 'var(--danger)';
            });
    } else {
        syncStatus.innerText = 'Chưa kết nối Cloud';
        syncStatus.style.color = 'var(--danger)';
    }
}

function setSystemMode(mode) {
    sendCommand({ mode: mode });
}

// ==============================================================================
// 4b. CẤU HÌNH CHUYỂN ĐỘNG (servo continuous — thời gian chạy thực tế)
// ==============================================================================
const MOTION_DIRS = ['TL', 'TR', 'BL', 'BR'];

function applyMotionConfig(cfg) {
    if (!cfg) return;
    const setVal = (id, v) => {
        const el = document.getElementById(id);
        if (el && v !== undefined && v !== null) el.value = v;
    };
    MOTION_DIRS.forEach(d => {
        const key = d.toLowerCase();
        setVal('ms' + d, cfg[key + '_ms']);
        setVal('dir' + d, cfg[key + '_dir']);
    });
    setVal('cfgHomeMs', cfg.home_ms);
    setVal('cfgCwUs', cfg.cw_us);
    setVal('cfgCcwUs', cfg.ccw_us);
    setVal('cfgStopUs', cfg.stop_us);
}

function collectMotionConfig() {
    const num = (id, def) => {
        const el = document.getElementById(id);
        const v = el ? parseInt(el.value, 10) : NaN;
        return Number.isFinite(v) ? v : def;
    };
    const cfg = {
        home_ms: num('cfgHomeMs', 3000),
        cw_us: num('cfgCwUs', 2400),
        ccw_us: num('cfgCcwUs', 500),
        stop_us: num('cfgStopUs', 1500),
        timestamp: Date.now()
    };
    MOTION_DIRS.forEach(d => {
        const key = d.toLowerCase();
        cfg[key + '_ms'] = num('ms' + d, 0);
        cfg[key + '_dir'] = num('dir' + d, 1);
    });
    return cfg;
}

function setMotionStatus(text, ok) {
    const el = document.getElementById('motionStatus');
    if (!el) return;
    el.innerText = text;
    el.className = 'form-status ' + (ok ? 'ok' : 'danger');
}

function saveMotionConfig(quiet) {
    if (!firebaseDb) {
        setMotionStatus('Chưa kết nối Cloud — không lưu được.', false);
        return Promise.reject(new Error('no-db'));
    }
    // Gửi qua /command (rules cho phép ghi); ESP lưu NVS + echo lại trong telemetry
    cfgLockUntil = Date.now() + 5000;
    sendCommand(collectMotionConfig());
    if (!quiet) setMotionStatus('Đã gửi cấu hình. ESP áp dụng trong ~1s.', true);
    return Promise.resolve();
}

// Lưu cấu hình hiện tại rồi ra lệnh chạy: về home -> tới hướng đích
function runMotion(target) {
    setMotionStatus('Đang gửi lệnh ' + target + '...', true);
    cfgLockUntil = Date.now() + 5000;
    sendCommand(Object.assign(collectMotionConfig(), { mode: 'MANUAL', move: target }));
    setMotionStatus('Đã gửi: home → ' + target + '. Theo dõi "Trạng Thái Servo".', true);
}

function runHome() {
    sendCommand(Object.assign(collectMotionConfig(), { mode: 'MANUAL', move: 'HOME' }));
    setMotionStatus('Đang về home (TL).', true);
}

function stopServo() {
    sendCommand({ mode: 'MANUAL', stop: true });
    setMotionStatus('Đã gửi lệnh DỪNG.', true);
}

// -----------------------------------------------------------------------------
// ĐO THỜI GIAN: bấm Đo -> servo quay liên tục; bấm GHI -> dừng, ESP trả last_ms
// -----------------------------------------------------------------------------
let measuring = null;      // 'TL'|'TR'|'BL'|'BR'|'HOME' đang đo
let pendingStop = null;    // {target, seq} chờ telemetry báo số ms mới
let measureT0 = 0;
let measureTimer = null;
let lastMoveSeq = 0;
let autoHomeAfterRecord = true;
let cfgLockUntil = 0; // chặn echo telemetry ghi đè giá trị vừa lưu (5s)

function measureBtn(target) {
    return document.getElementById('btnMeasure' + target);
}

function setLiveText(text, active) {
    const el = document.getElementById('motionLive');
    if (!el) return;
    el.innerText = text;
    el.classList.toggle('active', !!active);
}

function startMeasure(target) {
    if (measuring) return;
    measuring = target;
    measureT0 = Date.now();
    const btn = measureBtn(target);
    if (btn) {
        btn.classList.add('recording');
        btn.innerText = '■ GHI';
    }
    setLiveText(`${target}: 0 ms — bấm ■ GHI khi vừa đúng vị trí`, true);
    measureTimer = setInterval(() => {
        setLiveText(`${measuring || target}: ${Date.now() - measureT0} ms — bấm ■ GHI khi vừa đúng vị trí`, true);
    }, 100);
    sendCommand({ mode: 'MANUAL', run: target });
}

function stopMeasure() {
    const target = measuring;
    if (!target) return;
    measuring = null;
    if (measureTimer) { clearInterval(measureTimer); measureTimer = null; }
    const btn = measureBtn(target);
    if (btn) {
        btn.classList.remove('recording');
        btn.innerText = '▶ Đo';
    }
    pendingStop = { target: target, seq: lastMoveSeq };
    sendCommand({ mode: 'MANUAL', stop: true });
    setLiveText(`${target}: đang chờ số ms từ ESP...`, true);
}

function toggleMeasure(target) {
    if (measuring === target) stopMeasure();
    else if (!measuring) startMeasure(target);
    else setMotionStatus(`Đang đo ${measuring}. Bấm ■ GHI để dừng trước.`, false);
}

function abortMeasure() { // nút Dừng: hủy đo, không ghi số
    if (measuring) {
        const btn = measureBtn(measuring);
        if (btn) { btn.classList.remove('recording'); btn.innerText = '▶ Đo'; }
        measuring = null;
        if (measureTimer) { clearInterval(measureTimer); measureTimer = null; }
    }
    pendingStop = null;
}

// Gọi từ onTelemetryReceived: nhận last_ms của lần chạy vừa dừng
function handleMotionTelemetry(data) {
    if (typeof data.move_seq === 'number') {
        if (data.move_seq > lastMoveSeq) lastMoveSeq = data.move_seq;
    }
    if (!pendingStop) return;
    if (data.moving === 1) return;
    if (typeof data.move_seq !== 'number') return;
    if (data.move_seq <= pendingStop.seq) return;
    if (data.last_target !== pendingStop.target) return;

    const ms = parseInt(data.last_ms, 10) || 0;
    const target = pendingStop.target;
    pendingStop = null;
    if (ms < 50) {
        setLiveText(`${target}: số đo không hợp lệ (${ms} ms)`, false);
        return;
    }

    if (target === 'HOME') document.getElementById('cfgHomeMs').value = ms;
    else document.getElementById('ms' + target).value = ms;

    setLiveText(`✔ ${target} = ${ms} ms — đã lưu`, false);
    cfgLockUntil = Date.now() + 5000;

    const homeMs = parseInt(document.getElementById('cfgHomeMs').value, 10) || 0;
    if (autoHomeAfterRecord && target !== 'HOME' && homeMs > 0) {
        sendCommand(Object.assign(collectMotionConfig(), { mode: 'MANUAL', move: 'HOME' }));
        setMotionStatus(`✔ Đã lưu ${target} = ${ms} ms. Đang tự về home...`, true);
    } else {
        sendCommand(collectMotionConfig());
        if (autoHomeAfterRecord && target !== 'HOME') {
            setMotionStatus(`✔ Đã lưu ${target} = ${ms} ms. Chưa có home_ms → hãy đo Home trước.`, false);
        } else {
            setMotionStatus(`✔ Đã lưu ${target} = ${ms} ms vào ESP.`, true);
        }
    }
}

// Chỉ dùng cho mô phỏng 3D (servo continuous không có góc thật)
function targetToAngle(target) {
    switch (target) {
        case 'TL': return 90;   // Đông
        case 'BL': return 180;  // Nam
        case 'TR': return 270;  // Tây
        case 'BR': return 0;    // Bắc
        default: return 90;
    }
}

// ==============================================================================
// 5. MODAL CÀI ĐẶT
// ==============================================================================
function setupUIEventListeners() {
    document.getElementById('btnOpenConfig').onclick = openConfigModal;
    document.getElementById('btnCloseConfig').onclick = closeConfigModal;
    document.getElementById('btnCancelConfig').onclick = closeConfigModal;

    document.getElementById('btnSaveConfig').onclick = () => {
        saveConfig({ firebaseUrl: document.getElementById('cfgFbUrl').value.trim() });
        closeConfigModal();
        startConnection();
    };

    document.getElementById('btnOpenWifi').onclick = openWifiModal;
    document.getElementById('btnCloseWifi').onclick = closeWifiModal;
    document.getElementById('btnCancelWifi').onclick = closeWifiModal;

    const btnSaveMotion = document.getElementById('btnSaveMotion');
    if (btnSaveMotion) btnSaveMotion.onclick = () => saveMotionConfig(false);
    const btnRunHome = document.getElementById('btnRunHome');
    if (btnRunHome) btnRunHome.onclick = runHome;
    const btnStopServo = document.getElementById('btnStopServo');
    if (btnStopServo) btnStopServo.onclick = () => { abortMeasure(); stopServo(); };

    document.getElementById('btnSaveWifi').onclick = () => {
        const ssid = document.getElementById('wifiSsid').value.trim();
        const pass = document.getElementById('wifiPass').value;
        const status = document.getElementById('wifiFormStatus');

        if (!ssid) {
            status.innerText = 'SSID không được để trống.';
            status.className = 'form-status danger';
            return;
        }
        if (!firebaseDb) {
            status.innerText = 'Chưa kết nối Cloud — không thể gửi.';
            status.className = 'form-status danger';
            return;
        }

        sendCommand({ wifi_ssid: ssid, wifi_pass: pass });
        status.innerText = 'Đã gửi. ESP thử kết nối tối đa 20s — sai mật khẩu sẽ tự khôi phục WiFi cũ. Mật khẩu bị xóa khỏi Cloud sau khi áp dụng.';
        status.className = 'form-status ok';
        document.getElementById('wifiPass').value = '';
    };
}

function openConfigModal() {
    document.getElementById('configModal').classList.add('active');
}

function closeConfigModal() {
    document.getElementById('configModal').classList.remove('active');
}

function applyConfigToModal() {
    document.getElementById('cfgFbUrl').value = config.firebaseUrl;
}

function openWifiModal() {
    document.getElementById('wifiModal').classList.add('active');
}

function closeWifiModal() {
    document.getElementById('wifiModal').classList.remove('active');
}

// ==============================================================================
// 6. 3D VISUALIZER (THREE.JS) — CHỈ MÔ PHỎNG GÓC THẬT, KHÔNG MẶT TRỜI ẢO
// ==============================================================================
let scene3D, camera3D, renderer3D, controls3D;
let trackerRotatingGroup;
let ldrMeshes = {};
let current3DAngle = 90;
let target3DAngle = 90;

function init3DVisualizer() {
    const container = document.getElementById('tracker3DContainer');
    if (!container) return;

    const width = container.clientWidth || 340;
    const height = container.clientHeight || 230;

    scene3D = new THREE.Scene();

    camera3D = new THREE.PerspectiveCamera(38, width / height, 0.1, 100);
    camera3D.position.set(6.5, 6.0, 7.5);

    renderer3D = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    renderer3D.setSize(width, height);
    renderer3D.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    renderer3D.shadowMap.enabled = true;
    renderer3D.shadowMap.type = THREE.PCFSoftShadowMap;
    container.insertBefore(renderer3D.domElement, container.firstChild);

    if (THREE.OrbitControls) {
        controls3D = new THREE.OrbitControls(camera3D, renderer3D.domElement);
        controls3D.enableDamping = true;
        controls3D.dampingFactor = 0.08;
        controls3D.maxPolarAngle = Math.PI / 2 + 0.1;
        controls3D.minDistance = 3;
        controls3D.maxDistance = 25;
        controls3D.target.set(0, 1.3, 0);
    }

    scene3D.add(new THREE.AmbientLight(0x38bdf8, 0.6));
    scene3D.add(new THREE.HemisphereLight(0xffffff, 0x0f172a, 0.6));

    // Đèn chiếu cố định (không phải vị trí mặt trời giả)
    const dirLight = new THREE.DirectionalLight(0xfff3cc, 2.0);
    dirLight.position.set(6, 10, 4);
    dirLight.castShadow = true;
    dirLight.shadow.mapSize.width = 1024;
    dirLight.shadow.mapSize.height = 1024;
    dirLight.shadow.camera.near = 0.5;
    dirLight.shadow.camera.far = 30;
    dirLight.shadow.camera.left = -5;
    dirLight.shadow.camera.right = 5;
    dirLight.shadow.camera.top = 5;
    dirLight.shadow.camera.bottom = -5;
    scene3D.add(dirLight);

    build3DSceneModels();

    const resizeObserver = new ResizeObserver(() => {
        if (!container || !renderer3D || !camera3D) return;
        const w = container.clientWidth;
        const h = container.clientHeight;
        if (w > 0 && h > 0) {
            camera3D.aspect = w / h;
            camera3D.updateProjectionMatrix();
            renderer3D.setSize(w, h);
        }
    });
    resizeObserver.observe(container);

    animate3D();
}

function build3DSceneModels() {
    const ground = new THREE.Mesh(
        new THREE.CylinderGeometry(3.6, 3.8, 0.15, 32),
        new THREE.MeshStandardMaterial({ color: 0x0f172a, roughness: 0.6, metalness: 0.8 })
    );
    ground.position.y = -0.075;
    ground.receiveShadow = true;
    scene3D.add(ground);

    const ring = new THREE.Mesh(
        new THREE.RingGeometry(3.3, 3.45, 48),
        new THREE.MeshBasicMaterial({ color: 0x38bdf8, side: THREE.DoubleSide })
    );
    ring.rotation.x = -Math.PI / 2;
    ring.position.y = 0.01;
    scene3D.add(ring);

    const grid = new THREE.GridHelper(6, 12, 0x38bdf8, 0x1e293b);
    grid.position.y = 0.01;
    scene3D.add(grid);

    const baseGroup = new THREE.Group();

    const basePod = new THREE.Mesh(
        new THREE.CylinderGeometry(0.7, 0.9, 0.4, 24),
        new THREE.MeshStandardMaterial({ color: 0x1e293b, metalness: 0.9, roughness: 0.3 })
    );
    basePod.position.y = 0.2;
    basePod.castShadow = true;
    baseGroup.add(basePod);

    const mast = new THREE.Mesh(
        new THREE.CylinderGeometry(0.12, 0.12, 1.4, 16),
        new THREE.MeshStandardMaterial({ color: 0x94a3b8, metalness: 0.95, roughness: 0.2 })
    );
    mast.position.y = 1.0;
    mast.castShadow = true;
    baseGroup.add(mast);

    const servo = new THREE.Mesh(
        new THREE.BoxGeometry(0.5, 0.6, 0.5),
        new THREE.MeshStandardMaterial({ color: 0x0284c7, metalness: 0.5, roughness: 0.4 })
    );
    servo.position.y = 1.8;
    servo.castShadow = true;
    baseGroup.add(servo);

    scene3D.add(baseGroup);

    trackerRotatingGroup = new THREE.Group();
    trackerRotatingGroup.position.set(0, 2.15, 0);
    scene3D.add(trackerRotatingGroup);

    const horn = new THREE.Mesh(
        new THREE.CylinderGeometry(0.25, 0.25, 0.1, 16),
        new THREE.MeshStandardMaterial({ color: 0xf59e0b, metalness: 0.8, roughness: 0.3 })
    );
    horn.position.y = 0.05;
    trackerRotatingGroup.add(horn);

    const arm = new THREE.Mesh(
        new THREE.BoxGeometry(0.15, 0.4, 0.15),
        new THREE.MeshStandardMaterial({ color: 0x64748b, metalness: 0.7, roughness: 0.3 })
    );
    arm.position.set(0, 0.25, 0);
    trackerRotatingGroup.add(arm);

    const panelAssembly = new THREE.Group();
    panelAssembly.position.set(0, 0.45, 0);
    panelAssembly.rotation.x = THREE.MathUtils.degToRad(35);

    const frame = new THREE.Mesh(
        new THREE.BoxGeometry(2.4, 0.08, 1.8),
        new THREE.MeshStandardMaterial({ color: 0x334155, metalness: 0.85, roughness: 0.25 })
    );
    frame.castShadow = true;
    frame.receiveShadow = true;
    panelAssembly.add(frame);

    const pvCells = new THREE.Mesh(
        new THREE.BoxGeometry(2.28, 0.02, 1.68),
        new THREE.MeshStandardMaterial({
            color: 0x0b1e48,
            roughness: 0.2,
            metalness: 0.7,
            emissive: 0x051336
        })
    );
    pvCells.position.y = 0.045;
    pvCells.receiveShadow = true;
    panelAssembly.add(pvCells);

    const baffleMat = new THREE.MeshStandardMaterial({ color: 0x1e293b, roughness: 0.5 });
    const baffleH = new THREE.Mesh(new THREE.BoxGeometry(0.9, 0.28, 0.04), baffleMat);
    baffleH.position.set(0, 0.18, 0);
    baffleH.castShadow = true;
    panelAssembly.add(baffleH);

    const baffleV = new THREE.Mesh(new THREE.BoxGeometry(0.04, 0.28, 0.9), baffleMat);
    baffleV.position.set(0, 0.18, 0);
    baffleV.castShadow = true;
    panelAssembly.add(baffleV);

    const ldrBaseGeo = new THREE.CylinderGeometry(0.06, 0.06, 0.08, 12);
    const ldrBaseMat = new THREE.MeshStandardMaterial({ color: 0x111827 });
    const ldrOffset = 0.25;
    const ldrPositions = {
        tl: [-ldrOffset, 0.08, -ldrOffset],
        tr: [ldrOffset, 0.08, -ldrOffset],
        bl: [-ldrOffset, 0.08, ldrOffset],
        br: [ldrOffset, 0.08, ldrOffset]
    };

    for (const key in ldrPositions) {
        const ldrGroup = new THREE.Group();
        ldrGroup.position.set(...ldrPositions[key]);
        ldrGroup.add(new THREE.Mesh(ldrBaseGeo, ldrBaseMat));

        const capMat = new THREE.MeshStandardMaterial({
            color: 0xf59e0b,
            emissive: 0xf59e0b,
            emissiveIntensity: 0.8,
            roughness: 0.3
        });
        const cap = new THREE.Mesh(new THREE.SphereGeometry(0.05, 12, 12), capMat);
        cap.position.y = 0.05;
        ldrGroup.add(cap);

        panelAssembly.add(ldrGroup);
        ldrMeshes[key] = capMat;
    }

    trackerRotatingGroup.add(panelAssembly);
}

function update3DTracker(angleDeg, ldrData) {
    if (typeof angleDeg === 'number') {
        target3DAngle = angleDeg;
    }

    if (ldrData && ldrMeshes.tl) {
        const maxAdc = 4095;
        ldrMeshes.tl.emissiveIntensity = Math.min(2.5, (ldrData.ldr_tl / maxAdc) * 2.5);
        ldrMeshes.tr.emissiveIntensity = Math.min(2.5, (ldrData.ldr_tr / maxAdc) * 2.5);
        ldrMeshes.bl.emissiveIntensity = Math.min(2.5, (ldrData.ldr_bl / maxAdc) * 2.5);
        ldrMeshes.br.emissiveIntensity = Math.min(2.5, (ldrData.ldr_br / maxAdc) * 2.5);
    }
}

function animate3D() {
    requestAnimationFrame(animate3D);

    current3DAngle += (target3DAngle - current3DAngle) * 0.08;

    if (trackerRotatingGroup) {
        // Servo 360°: 0°=Bắc, 90°=Đông, 180°=Nam, 270°=Tây
        const rotationRad = THREE.MathUtils.degToRad(-current3DAngle);
        trackerRotatingGroup.rotation.y = rotationRad;
    }

    const hudAngle = document.getElementById('hud3DAngle');
    if (hudAngle) {
        hudAngle.innerText = current3DAngle.toFixed(1);
    }

    if (controls3D) controls3D.update();
    if (renderer3D && scene3D && camera3D) {
        renderer3D.render(scene3D, camera3D);
    }
}

function set3DCameraView(view) {
    document.querySelectorAll('.btn-hud').forEach(b => b.classList.remove('active'));
    if (!camera3D || !controls3D) return;

    if (view === 'orbit') {
        document.getElementById('btnViewOrbit')?.classList.add('active');
        camera3D.position.set(6.5, 6.0, 7.5);
        controls3D.target.set(0, 1.3, 0);
    } else if (view === 'top') {
        document.getElementById('btnViewTop')?.classList.add('active');
        camera3D.position.set(0, 11, 0.01);
        controls3D.target.set(0, 1.5, 0);
    }
    controls3D.update();
}

function reset3DCamera() {
    set3DCameraView('orbit');
}
