/**
 * Oculus Web UI — simplified, robust version.
 */

const MOVEMENTS = [
    'abduction', 'forward_flexion', 'extension',
    'external_rotation', 'internal_rotation', 'adduction',
    'horizontal_adduction', 'scapular_protraction',
    'scapular_retraction', 'shoulder_elevation', 'shoulder_depression',
];

const LEFT_INDICES = new Set([5, 7, 9, 11]);
const RIGHT_INDICES = new Set([6, 8, 10, 12]);
const SHOULDER_ROM_INDICES = new Set([5, 6, 7, 8, 9, 10, 11, 12]);
const SKELETON_BONES = [[5,6],[5,7],[7,9],[6,8],[8,10],[5,11],[6,12],[11,12]];

// State
let latestKeypoints = null;
let latestAngles = null;
let capturedFrameA = null;
let capturedFrameB = null;
let selectedSide = 'left';
let isLive = true;
let frameWidth = 640;
let frameHeight = 360;
let compare3d = null;
let realtimeAngleChart = null;

// ── Helpers ──

function flattenAngles(data) {
    const flat = {};
    const angles = data?.angles || data || {};
    for (const [key, val] of Object.entries(angles)) {
        if (typeof val === 'object' && val !== null) {
            for (const [sub, v] of Object.entries(val)) {
                flat[`${key}_${sub}`] = v;
            }
        }
    }
    return flat;
}

function isSideVisible(idx) {
    if (!SHOULDER_ROM_INDICES.has(idx)) return false;
    if (selectedSide === 'both') return true;
    if (selectedSide === 'left') return LEFT_INDICES.has(idx);
    return RIGHT_INDICES.has(idx);
}

function showToast(msg) {
    let c = document.getElementById('toast-container');
    if (!c) {
        c = document.createElement('div');
        c.id = 'toast-container';
        c.style.cssText = 'position:fixed;top:20px;right:20px;z-index:9999';
        document.body.appendChild(c);
    }
    const t = document.createElement('div');
    t.style.cssText = 'background:var(--bg-card);color:var(--text-primary);padding:10px 16px;border-radius:8px;margin-bottom:8px;font-size:0.85rem;box-shadow:0 4px 12px rgba(0,0,0,0.3)';
    t.textContent = msg;
    c.appendChild(t);
    setTimeout(() => t.remove(), 3000);
}

function updateConnectionStatus(connected) {
    const dot = document.getElementById('ws-status');
    const text = document.getElementById('ws-text');
    if (dot) dot.className = 'status-dot ' + (connected ? 'connected' : 'disconnected');
    if (text) text.textContent = connected ? 'Connected' : 'Disconnected';
}

// ── 3D Skeleton (Live) ──

function initSkeleton3D() {
    const container = document.getElementById('skeleton3d-container');
    const canvas = document.getElementById('skeleton3d-canvas');
    if (!container || !canvas) return;

    const w = container.clientWidth || 400;
    const h = container.clientHeight || 400;

    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x0f1419);

    const camera = new THREE.PerspectiveCamera(60, w / h, 0.1, 1000);
    camera.position.set(0, 1, 2);
    camera.lookAt(0, 1, 0);

    const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
    renderer.setSize(w, h);

    const controls = new THREE.OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;
    controls.target.set(0, 1, 0);

    scene.add(new THREE.GridHelper(2, 10, 0x2C3E50, 0x1A2332));

    const group = new THREE.Group();
    scene.add(group);

    const geo = new THREE.SphereGeometry(0.015, 8, 8);
    const matLeft = new THREE.MeshBasicMaterial({ color: 0x4FC3F7 });
    const matRight = new THREE.MeshBasicMaterial({ color: 0xFFB74D });
    const lineMatLeft = new THREE.LineBasicMaterial({ color: 0x4FC3F7 });
    const lineMatRight = new THREE.LineBasicMaterial({ color: 0xFFB74D });

    function animate() {
        requestAnimationFrame(animate);
        controls.update();
        renderer.render(scene, camera);
    }
    animate();

    compare3d = { scene, camera, renderer, controls, group, geo, matLeft, matRight, lineMatLeft, lineMatRight };
}

function updateSkeleton3D(keypoints) {
    if (!compare3d || !keypoints || keypoints.length < 17) return;

    const { group, geo, matLeft, matRight, lineMatLeft, lineMatRight } = compare3d;
    const scale = 0.003;
    const centerY = 1.0;

    while (group.children.length > 0) group.remove(group.children[0]);

    for (let i = 0; i < 17; i++) {
        const kp = keypoints[i];
        if (!kp || kp.confidence < 0.3 || !isSideVisible(i)) continue;

        const isLeft = LEFT_INDICES.has(i);
        const s = new THREE.Mesh(geo, isLeft ? matLeft : matRight);
        s.position.set((kp.x - 320) * scale, centerY - (kp.y - 180) * scale, 0);
        group.add(s);
    }

    for (const [i, j] of SKELETON_BONES) {
        const a = keypoints[i], b = keypoints[j];
        if (!a || !b || a.confidence < 0.3 || b.confidence < 0.3) continue;
        if (!isSideVisible(i) && !isSideVisible(j)) continue;

        const isLeft = LEFT_INDICES.has(i);
        const pts = [
            new THREE.Vector3((a.x - 320) * scale, centerY - (a.y - 180) * scale, 0),
            new THREE.Vector3((b.x - 320) * scale, centerY - (b.y - 180) * scale, 0),
        ];
        group.add(new THREE.Line(new THREE.BufferGeometry().setFromPoints(pts), isLeft ? lineMatLeft : lineMatRight));
    }
}

// ── 3D Compare ──

function updateCompare3D(kpA, kpB) {
    if (!compare3d || !kpA || !kpB) return;

    const { group, geo, matLeft, matRight, lineMatLeft, lineMatRight } = compare3d;
    const scale = 0.003;

    while (group.children.length > 0) group.remove(group.children[0]);

    const si = selectedSide === 'right' ? 6 : 5;
    const ei = selectedSide === 'right' ? 8 : 7;
    const wi = selectedSide === 'right' ? 10 : 9;

    function drawArm(kp, mat, lineMat) {
        const s = kp[si], e = kp[ei], w = kp[wi];
        if (!s || !e || !w || s.confidence < 0.3) return null;

        const shoulder = new THREE.Vector3(0, 0, 0);
        const elbow = new THREE.Vector3((e.x - s.x) * scale, -(e.y - s.y) * scale, 0);
        const wrist = new THREE.Vector3((w.x - s.x) * scale, -(w.y - s.y) * scale, 0);

        for (const p of [shoulder, elbow, wrist]) {
            const sp = new THREE.Mesh(geo, mat);
            sp.position.copy(p);
            group.add(sp);
        }

        for (const [a, b] of [[shoulder, elbow], [elbow, wrist]]) {
            group.add(new THREE.Line(
                new THREE.BufferGeometry().setFromPoints([a, b]), lineMat));
        }

        // Angle at shoulder (relative to vertical)
        const v1 = elbow.clone().sub(shoulder);
        const v2 = new THREE.Vector3(0, -1, 0);
        const mag = v1.length();
        const cosA = mag > 0 ? Math.max(-1, Math.min(1, v1.dot(v2) / mag)) : 0;
        return { angle: Math.acos(cosA) * 180 / Math.PI };
    }

    const armA = drawArm(kpA, matLeft, lineMatLeft);
    const armB = drawArm(kpB, matRight, lineMatRight);

    // Show ROM label
    const label = document.getElementById('compare3d-rom-label');
    if (label && armA && armB) {
        const fa = flattenAngles(capturedFrameA?.angles);
        const fb = flattenAngles(capturedFrameB?.angles);
        const mv = document.getElementById('measure-movement')?.value || 'forward_flexion';
        const side = selectedSide === 'right' ? 'right' : 'left';
        const valA = fa[`${side}_${mv}`] ?? armA.angle;
        const valB = fb[`${side}_${mv}`] ?? armB.angle;
        label.innerHTML = `A: ${valA.toFixed(1)}° | B: ${valB.toFixed(1)}° | ROM: ${(valB - valA).toFixed(1)}°`;
    }
}

// ── Video + Skeleton Overlay ──

function drawSkeletonOverlay(ctx, keypoints, w, h) {
    if (!keypoints || keypoints.length < 17) return;

    const sx = w / frameWidth;
    const sy = h / frameHeight;

    // Bones
    for (const [i, j] of SKELETON_BONES) {
        if (!isSideVisible(i) && !isSideVisible(j)) continue;
        const a = keypoints[i], b = keypoints[j];
        if (a.confidence <= 0.3 || b.confidence <= 0.3) continue;

        const isLeft = LEFT_INDICES.has(i);
        ctx.strokeStyle = isLeft ? '#4FC3F7' : '#FFB74D';
        ctx.lineWidth = 3;
        ctx.beginPath();
        ctx.moveTo(a.x * sx, a.y * sy);
        ctx.lineTo(b.x * sx, b.y * sy);
        ctx.stroke();
    }

    // Keypoints
    for (let i = 0; i < keypoints.length; i++) {
        if (!isSideVisible(i)) continue;
        const kp = keypoints[i];
        if (kp.confidence <= 0.3) continue;

        ctx.beginPath();
        ctx.arc(kp.x * sx, kp.y * sy, 5, 0, Math.PI * 2);
        ctx.fillStyle = LEFT_INDICES.has(i) ? '#4FC3F7' : '#FFB74D';
        ctx.fill();
    }
}

// ── Main Init ──

function initLiveView() {
    const canvas = document.getElementById('video-canvas');
    const placeholder = document.getElementById('video-placeholder');
    const ctx = canvas?.getContext('2d');
    let frameImg = new Image();

    frameImg.onload = () => {
        if (canvas.width !== frameImg.width) canvas.width = frameImg.width;
        if (canvas.height !== frameImg.height) canvas.height = frameImg.height;
        ctx.drawImage(frameImg, 0, 0);
        drawSkeletonOverlay(ctx, latestKeypoints, canvas.width, canvas.height);
        if (placeholder) placeholder.style.display = 'none';
    };

    // 3D init
    initSkeleton3D();

    // Chart
    if (typeof ChartUtils !== 'undefined') {
        realtimeAngleChart = ChartUtils.createRealtimeChart('angle-chart', {
            title: 'Abduction L/R', yLabel: 'Angle (°)', maxPoints: 120,
        });
    }

    // Side selector
    function setSide(side) {
        selectedSide = side;
        document.querySelectorAll('[id^="btn-side-"]').forEach(b => b.classList.remove('active'));
        const btn = document.getElementById(`btn-side-${side}`);
        if (btn) btn.classList.add('active');
        const ind = document.getElementById('side-indicator');
        if (ind) {
            const colors = { left: '#4FC3F7', right: '#FFB74D', both: '#38bdf8' };
            ind.innerHTML = `<span style="color:${colors[side]}">●</span> ${side.toUpperCase()}`;
        }
    }
    document.getElementById('btn-side-left')?.addEventListener('click', () => setSide('left'));
    document.getElementById('btn-side-right')?.addEventListener('click', () => setSide('right'));
    document.getElementById('btn-side-both')?.addEventListener('click', () => setSide('both'));

    // Start/Stop
    let isLive = true;
    const btnSS = document.getElementById('btn-start-stop');
    btnSS?.addEventListener('click', () => {
        isLive = !isLive;
        btnSS.textContent = isLive ? '⏸ Stop' : '▶ Start';
        btnSS.style.background = isLive ? '#2ECC71' : '#F5A623';
        showToast(isLive ? 'Live resumed' : 'Live paused');
    });

    // Capture A
    document.getElementById('btn-capture-a')?.addEventListener('click', () => {
        capturedFrameA = {
            keypoints: latestKeypoints ? JSON.parse(JSON.stringify(latestKeypoints)) : null,
            angles: latestAngles ? JSON.parse(JSON.stringify(latestAngles)) : null,
            time: new Date().toLocaleTimeString()
        };
        const st = document.getElementById('capture-status');
        if (st) st.innerHTML = `<span style="color:#2ECC71">✓ A captured (${capturedFrameA.time})</span>`;
        showToast('Frame A captured');
    });

    // Capture B
    document.getElementById('btn-capture-b')?.addEventListener('click', () => {
        capturedFrameB = {
            keypoints: latestKeypoints ? JSON.parse(JSON.stringify(latestKeypoints)) : null,
            angles: latestAngles ? JSON.parse(JSON.stringify(latestAngles)) : null,
            time: new Date().toLocaleTimeString()
        };
        const st = document.getElementById('capture-status');
        if (st) st.innerHTML = `<span style="color:#2ECC71">✓ A + B captured</span>`;
        showToast('Frame B captured');
    });

    // Measure
    document.getElementById('btn-measure')?.addEventListener('click', () => {
        if (!capturedFrameA || !capturedFrameB) { showToast('Capture A and B first'); return; }
        const movement = document.getElementById('measure-movement')?.value;
        if (!movement) { showToast('Select movement'); return; }

        const fa = flattenAngles(capturedFrameA.angles);
        const fb = flattenAngles(capturedFrameB.angles);
        const side = selectedSide;
        const valA = fa[`${side}_${movement}`] ?? 0;
        const valB = fb[`${side}_${movement}`] ?? 0;
        const rom = valB - valA;
        const abs = Math.abs(rom);
        const severity = abs < 10 ? 'Normal' : abs < 30 ? 'Mild' : abs < 60 ? 'Moderate' : 'Severe';
        const color = abs < 10 ? '#2ECC71' : abs < 30 ? '#F5A623' : '#E74C3C';
        const label = movement.replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase());

        const res = document.getElementById('measure-result');
        if (res) {
            res.innerHTML = `
                <div style="background:var(--bg-card);border-radius:8px;padding:1rem;border-left:4px solid ${color}">
                    <div style="font-size:0.75rem;color:var(--text-secondary);margin-bottom:0.5rem">${label} — ${side.toUpperCase()}</div>
                    <div style="display:flex;justify-content:space-between;margin-bottom:0.5rem">
                        <div style="text-align:center">
                            <div style="font-size:0.7rem;color:var(--text-secondary)">A (Rest)</div>
                            <div style="font-size:1.2rem;font-weight:700;color:#4FC3F7">${valA.toFixed(1)}°</div>
                        </div>
                        <div style="text-align:center">
                            <div style="font-size:0.7rem;color:var(--text-secondary)">ROM</div>
                            <div style="font-size:1.5rem;font-weight:700;color:${color}">${rom > 0 ? '+' : ''}${rom.toFixed(1)}°</div>
                        </div>
                        <div style="text-align:center">
                            <div style="font-size:0.7rem;color:var(--text-secondary)">B (Max)</div>
                            <div style="font-size:1.2rem;font-weight:700;color:#FFB74D">${valB.toFixed(1)}°</div>
                        </div>
                    </div>
                    <div style="text-align:center;font-size:0.8rem;color:${color}">${severity}</div>
                </div>`;
        }

        // Update 3D comparison
        const c3d = document.getElementById('compare3d-container');
        const live3d = document.getElementById('skeleton3d-container');
        if (c3d) c3d.style.display = '';
        if (live3d) live3d.style.display = 'none';
        if (compare3d) {
            // Resize renderer when shown
            if (c3d) {
                const w = c3d.clientWidth || 400;
                const h = c3d.clientHeight || 400;
                compare3d.renderer.setSize(w, h);
                compare3d.camera.aspect = w / h;
                compare3d.camera.updateProjectionMatrix();
            }
            updateCompare3D(capturedFrameA.keypoints, capturedFrameB.keypoints);
        }
        showToast(`${label} ROM: ${rom.toFixed(1)}° (${severity})`);
    });

    // Reset
    document.getElementById('btn-reset')?.addEventListener('click', () => {
        capturedFrameA = null;
        capturedFrameB = null;
        const res = document.getElementById('measure-result');
        if (res) res.innerHTML = '';
        const st = document.getElementById('capture-status');
        if (st) st.innerHTML = 'Press A at resting, B at max ROM';
        // Switch back to live 3D
        const c3d = document.getElementById('compare3d-container');
        const live3d = document.getElementById('skeleton3d-container');
        if (c3d) c3d.style.display = 'none';
        if (live3d) live3d.style.display = '';
        showToast('Reset');
    });

    // SSE connection
    const es = new EventSource('/ws/rom');
    es.onopen = () => updateConnectionStatus(true);
    es.onerror = () => updateConnectionStatus(false);

    es.onmessage = (e) => {
        if (!isLive) return;
        try {
            const data = JSON.parse(e.data);

            // Video frame (base64 JPEG)
            if (data.frame && typeof data.frame === 'string') {
                frameImg.src = 'data:image/jpeg;base64,' + data.frame;
            }

            // Keypoints
            if (data.keypoints && Array.isArray(data.keypoints)) {
                latestKeypoints = data.keypoints;
                if (data.frame_width) frameWidth = data.frame_width;
                if (data.frame_height) frameHeight = data.frame_height;
                if (compare3d) updateSkeleton3D(data.keypoints);
            }

            // Angles
            if (data.angles && typeof data.angles === 'object') {
                latestAngles = data;
                if (realtimeAngleChart && typeof ChartUtils !== 'undefined') {
                    const flat = flattenAngles(data);
                    const l = flat['left_abduction'] ?? 0;
                    const r = flat['right_abduction'] ?? 0;
                    ChartUtils.pushChartData(realtimeAngleChart, new Date().toLocaleTimeString(), [l, r]);
                }
            }
        } catch (err) {}
    };
}

// ── Init on load ──
document.addEventListener('DOMContentLoaded', () => {
    initLiveView();
});