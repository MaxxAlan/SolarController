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
    const sourceLabel = document.getElementById('sourceLabel');
    const netLabel = document.getElementById('netLabel');
    const chipNet = document.getElementById('chipNet');
    sourceLabel.innerText = 'Nguồn: Firebase Cloud';
    netLabel.innerText = 'Đang đồng bộ...';
    chipNet.querySelector('.dot').className = 'dot dot-amber';

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

    const netLabel = document.getElementById('netLabel');
    if (netLabel.innerText.includes('chưa có dữ liệu')) {
        netLabel.innerText = 'Cloud Online';
    }

    // 1. Góc servo (angle) + mode
    const angle = data.angle;
    document.getElementById('valCurAngle').innerText = angle;
    update3DTracker(angle, data);

    const tagMode = document.getElementById('tagMode');
    const isAuto = data.mode === 'AUTO';
    tagMode.innerText = isAuto ? 'AUTO TRACKING' : 'MANUAL CONTROL';
    tagMode.style.borderColor = isAuto ? 'var(--primary)' : 'var(--accent)';
    tagMode.style.color = isAuto ? 'var(--primary-bright)' : 'var(--accent)';

    const btnAuto = document.getElementById('btnAutoMode');
    const btnManual = document.getElementById('btnManualMode');
    const manualSection = document.getElementById('manualSection');
    if (isAuto) {
        btnAuto.classList.add('active');
        btnManual.classList.remove('active');
        manualSection.classList.add('disabled');
    } else {
        btnAuto.classList.remove('active');
        btnManual.classList.add('active');
        manualSection.classList.remove('disabled');
    }

    // 2. Pin: soc + voltage
    const soc = Math.max(0, Math.min(100, Math.round(data.soc)));
    document.getElementById('valSOC').innerText = soc;
    document.getElementById('valVoltage').innerText = Number(data.voltage).toFixed(2) + ' V';

    const fluid = document.getElementById('batteryFluid');
    fluid.style.height = soc + '%';
    if (soc > 40) {
        fluid.style.background = 'linear-gradient(180deg, #10b981 0%, #059669 100%)';
    } else if (soc > 20) {
        fluid.style.background = 'linear-gradient(180deg, #f59e0b 0%, #d97706 100%)';
    } else {
        fluid.style.background = 'linear-gradient(180deg, #ef4444 0%, #b91c1c 100%)';
    }

    // 3. LDR (4 ADC) + diff
    const maxADC = 4095;
    const ldr = { tl: data.ldr_tl, tr: data.ldr_tr, bl: data.ldr_bl, br: data.ldr_br };
    document.getElementById('valLdrTL').innerText = ldr.tl;
    document.getElementById('valLdrTR').innerText = ldr.tr;
    document.getElementById('valLdrBL').innerText = ldr.bl;
    document.getElementById('valLdrBR').innerText = ldr.br;

    document.getElementById('fillTL').style.width = ((ldr.tl / maxADC) * 100) + '%';
    document.getElementById('fillTR').style.width = ((ldr.tr / maxADC) * 100) + '%';
    document.getElementById('fillBL').style.width = ((ldr.bl / maxADC) * 100) + '%';
    document.getElementById('fillBR').style.width = ((ldr.br / maxADC) * 100) + '%';

    const clampedDiff = Math.max(-2000, Math.min(2000, data.diff));
    document.getElementById('diffNeedle').style.left = (((clampedDiff + 2000) / 4000) * 100) + '%';
    document.getElementById('valDiff').innerText = (data.diff > 0 ? '+' : '') + data.diff;

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

function handleSliderInput(val) {
    document.getElementById('lblSliderVal').innerText = val + '°';
}

function applyManualAngle(val) {
    sendCommand({ mode: 'MANUAL', angle: parseInt(val, 10) });
}

function sendPresetAngle(val) {
    document.getElementById('angleSliderInput').value = val;
    handleSliderInput(val);
    sendCommand({ mode: 'MANUAL', angle: val });
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
        const rotationRad = THREE.MathUtils.degToRad(-(current3DAngle - 90));
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
