/**
 * Oculus Web UI — main application logic.
 */

const API_BASE = '/api/v1';

const MOVEMENTS = [
    'abduction', 'forward_flexion', 'extension',
    'external_rotation', 'internal_rotation', 'adduction',
    'horizontal_adduction', 'scapular_protraction',
    'scapular_retraction', 'shoulder_elevation', 'shoulder_depression',
];

/* ── API helpers ── */

async function apiFetch(endpoint) {
    try {
        const res = await fetch(`${API_BASE}${endpoint}`);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        return await res.json();
    } catch (e) {
        console.error(`API error (${endpoint}):`, e);
        return null;
    }
}

async function getAngles() { return apiFetch('/rom/angles'); }
async function getMetrics() { return apiFetch('/rom/metrics'); }
async function getAnalysis() { return apiFetch('/rom/analysis'); }
async function getSessions() { return apiFetch('/sessions'); }
async function getSystemInfo() { return apiFetch('/system/info'); }

/* ── Status bar ── */

function updateConnectionStatus(connected) {
    const dot = document.getElementById('ws-status');
    const text = document.getElementById('ws-text');
    if (dot) {
        dot.className = 'status-dot ' + (connected ? 'connected' : 'disconnected');
    }
    if (text) {
        text.textContent = connected ? 'Connected' : 'Disconnected';
    }
}

/* ── Toast notifications ── */

function showToast(message, duration = 3000) {
    let container = document.getElementById('toast-container');
    if (!container) {
        container = document.createElement('div');
        container.id = 'toast-container';
        container.className = 'toast-container';
        document.body.appendChild(container);
    }
    const toast = document.createElement('div');
    toast.className = 'toast';
    toast.textContent = message;
    container.appendChild(toast);
    setTimeout(() => toast.remove(), duration);
}

/* ── Dashboard (index.html) ── */

async function initDashboard() {
    const info = await getSystemInfo();
    if (info) {
        setText('system-name', info.name || 'Oculus');
        setText('system-version', info.version || '—');
    }

    const metrics = await getMetrics();
    if (metrics) {
        setText('metric-sessions', metrics.total_sessions ?? '—');
        setText('metric-frames', metrics.total_frames ?? '—');
        setText('metric-uptime', formatUptime(metrics.uptime_seconds ?? 0));
    }

    loadSessionList();
}

async function loadSessionList() {
    const sessions = await getSessions();
    const container = document.getElementById('session-list');
    if (!container) return;
    if (!sessions || sessions.length === 0) {
        container.innerHTML = '<div class="session-item"><span class="session-info"><span class="session-meta">No sessions recorded</span></span></div>';
        return;
    }
    container.innerHTML = sessions.map(s => `
        <div class="session-item" onclick="window.location.href='analysis.html?session=${s.id}'">
            <div class="session-info">
                <div class="session-name">${escapeHtml(s.name || s.id)}</div>
                <div class="session-meta">${s.mode} &middot; ${formatTimestamp(s.start_time)}</div>
            </div>
            <div class="badge">${s.mode}</div>
        </div>
    `).join('');
}

/* ── Live view (live.html) ── */

let realtimeAngleChart = null;

// 3D Skeleton state
let skeleton3d = null;

function initSkeleton3D() {
    const container = document.getElementById('skeleton3d-container');
    if (!container) return;

    const width = container.clientWidth;
    const height = container.clientHeight;

    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x0f172a);

    const camera = new THREE.PerspectiveCamera(60, width / height, 0.1, 1000);
    camera.position.set(0, 1.2, 2.5);
    camera.lookAt(0, 1, 0);

    const renderer = new THREE.WebGLRenderer({
        canvas: document.getElementById('skeleton3d-canvas'),
        antialias: true
    });
    renderer.setSize(width, height);

    // OrbitControls for rotation
    const controls = new THREE.OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;
    controls.dampingFactor = 0.05;
    controls.target.set(0, 1, 0);

    // Grid
    const grid = new THREE.GridHelper(3, 15, 0x475569, 0x1e293b);
    scene.add(grid);

    // Skeleton group
    const skeletonGroup = new THREE.Group();
    scene.add(skeletonGroup);

    // Create spheres for keypoints
    const sphereGeo = new THREE.SphereGeometry(0.015, 12, 12);
    const materials = {
        left: new THREE.MeshBasicMaterial({ color: 0x4ade80 }),
        right: new THREE.MeshBasicMaterial({ color: 0xf87171 }),
        center: new THREE.MeshBasicMaterial({ color: 0x38bdf8 }),
    };

    const spheres = [];
    for (let i = 0; i < 17; i++) {
        const mat = (i % 2 === 1) ? materials.left :
                    (i % 2 === 0 && i > 0) ? materials.right :
                    materials.center;
        const sphere = new THREE.Mesh(sphereGeo, mat);
        sphere.visible = false;
        skeletonGroup.add(sphere);
        spheres.push(sphere);
    }

    // Bone connections
    const bones = [
        [0,1],[0,2],[1,3],[2,4],
        [5,6],
        [5,7],[7,9],
        [6,8],[8,10],
        [5,11],[6,12],
        [11,12],
        [11,13],[13,15],
        [12,14],[14,16]
    ];

    const lineMaterial = new THREE.LineBasicMaterial({ color: 0xfbbf24, linewidth: 3 });
    const lines = [];
    for (const [i, j] of bones) {
        const geometry = new THREE.BufferGeometry();
        const line = new THREE.Line(geometry, lineMaterial);
        line.visible = false;
        skeletonGroup.add(line);
        lines.push({ line, i, j });
    }

    // Animation loop
    function animate() {
        requestAnimationFrame(animate);
        controls.update();
        renderer.render(scene, camera);
    }
    animate();

    skeleton3d = { scene, camera, renderer, spheres, lines, skeletonGroup, controls };
}

function updateSkeleton3DComparison(kpA, kpB) {
    if (!skeleton3d || !kpA || !kpB) return;

    const { spheres, lines, skeletonGroup } = skeleton3d;
    const scale = 0.003;
    const centerY = 1.0;

    // Create Frame A spheres (green, semi-transparent)
    const matA = new THREE.MeshBasicMaterial({ color: 0x4ade80, transparent: true, opacity: 0.6 });
    const matB = new THREE.MeshBasicMaterial({ color: 0xf87171, transparent: true, opacity: 0.6 });
    const geo = new THREE.SphereGeometry(0.012, 8, 8);

    // Remove old comparison objects
    skeletonGroup.children.filter(c => c.userData?.comp).forEach(c => skeletonGroup.remove(c));

    // Add Frame A keypoints
    for (let i = 0; i < 17; i++) {
        if (kpA[i]?.confidence > 0.3) {
            const s = new THREE.Mesh(geo, matA);
            s.position.set((kpA[i].x - 320) * scale, centerY - (kpA[i].y - 180) * scale, 0);
            s.userData = { comp: true, frame: 'A' };
            skeletonGroup.add(s);
        }
    }

    // Add Frame B keypoints
    for (let i = 0; i < 17; i++) {
        if (kpB[i]?.confidence > 0.3) {
            const s = new THREE.Mesh(geo, matB);
            s.position.set((kpB[i].x - 320) * scale, centerY - (kpB[i].y - 180) * scale, 0);
            s.userData = { comp: true, frame: 'B' };
            skeletonGroup.add(s);
        }
    }

    // Draw movement arrows (A → B) for selected side
    const indices = selectedSide === 'left' ? [5, 7, 9] : [6, 8, 10];
    const arrowMat = new THREE.LineBasicMaterial({ color: 0xfbbf24 });
    for (const i of indices) {
        if (kpA[i]?.confidence > 0.3 && kpB[i]?.confidence > 0.3) {
            const points = [
                new THREE.Vector3((kpA[i].x - 320) * scale, centerY - (kpA[i].y - 180) * scale, 0),
                new THREE.Vector3((kpB[i].x - 320) * scale, centerY - (kpB[i].y - 180) * scale, 0),
            ];
            const lineGeo = new THREE.BufferGeometry().setFromPoints(points);
            const arrow = new THREE.Line(lineGeo, arrowMat);
            arrow.userData = { comp: true };
            skeletonGroup.add(arrow);
        }
    }
}

function updateSkeleton3D(keypoints) {
    if (!skeleton3d || !keypoints || keypoints.length < 17) return;

    const { spheres, lines } = skeleton3d;
    const scale = 0.003; // pixel to 3D unit
    const centerY = 1.0;

    // Update sphere positions (convert 2D to 3D with depth estimation)
    const refX = keypoints[6]?.x || 320; // right shoulder as reference
    const refY = keypoints[6]?.y || 180;

    for (let i = 0; i < 17; i++) {
        const kp = keypoints[i];
        if (kp.confidence > 0.3) {
            // Estimate depth from body position
            // Arms extended = further from body center = more depth
            let z = 0;
            if (i === 7 || i === 8) { // elbows
                z = Math.abs(kp.x - refX) * scale * 0.3;
            } else if (i === 9 || i === 10) { // wrists
                z = Math.abs(kp.x - refX) * scale * 0.5;
            }

            spheres[i].position.set(
                (kp.x - 320) * scale,
                centerY - (kp.y - 180) * scale,
                z
            );
            spheres[i].visible = true;
        } else {
            spheres[i].visible = false;
        }
    }

    // Update bone lines
    for (const { line, i, j } of lines) {
        if (spheres[i].visible && spheres[j].visible) {
            const positions = new Float32Array([
                spheres[i].position.x, spheres[i].position.y, spheres[i].position.z,
                spheres[j].position.x, spheres[j].position.y, spheres[j].position.z,
            ]);
            line.geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
            line.geometry.attributes.position.needsUpdate = true;
            line.visible = true;
        } else {
            line.visible = false;
        }
    }
}

// Skeleton overlay state
let latestKeypoints = null;
let latestAngles = {};
let capturedFrameA = null;  // { keypoints, angles, time }
let capturedFrameB = null;
let selectedSide = 'left';  // 'left' or 'right'
let comparisonActive = false;

// Crop boundary state (user-adjustable)
let cropRect = { x: 185, y: 0, w: 270, h: 360 };
let cropDragging = false;
let cropResizing = false;
let cropDragStart = { x: 0, y: 0 };
let cropResizeHandle = null; // 'tl','tr','bl','br','move'

function initCropEditor(canvas) {
    canvas.addEventListener('mousedown', (e) => {
        const rect = canvas.getBoundingClientRect();
        const mx = (e.clientX - rect.left) * canvas.width / rect.width;
        const my = (e.clientY - rect.top) * canvas.height / rect.height;

        // Check resize handles (8px corners)
        const handleSize = 8;
        if (Math.abs(mx - cropRect.x) < handleSize && Math.abs(my - cropRect.y) < handleSize) {
            cropResizing = true; cropResizeHandle = 'tl';
        } else if (Math.abs(mx - (cropRect.x + cropRect.w)) < handleSize && Math.abs(my - cropRect.y) < handleSize) {
            cropResizing = true; cropResizeHandle = 'tr';
        } else if (Math.abs(mx - cropRect.x) < handleSize && Math.abs(my - (cropRect.y + cropRect.h)) < handleSize) {
            cropResizing = true; cropResizeHandle = 'bl';
        } else if (Math.abs(mx - (cropRect.x + cropRect.w)) < handleSize && Math.abs(my - (cropRect.y + cropRect.h)) < handleSize) {
            cropResizing = true; cropResizeHandle = 'br';
        } else if (mx >= cropRect.x && mx <= cropRect.x + cropRect.w &&
                   my >= cropRect.y && my <= cropRect.y + cropRect.h) {
            cropDragging = true;
        }
        cropDragStart = { x: mx, y: my };
    });

    canvas.addEventListener('mousemove', (e) => {
        const rect = canvas.getBoundingClientRect();
        const mx = (e.clientX - rect.left) * canvas.width / rect.width;
        const my = (e.clientY - rect.top) * canvas.height / rect.height;
        const dx = mx - cropDragStart.x;
        const dy = my - cropDragStart.y;

        if (cropDragging) {
            cropRect.x = Math.max(0, Math.min(canvas.width - cropRect.w, cropRect.x + dx));
            cropRect.y = Math.max(0, Math.min(canvas.height - cropRect.h, cropRect.y + dy));
            cropDragStart = { x: mx, y: my };
        } else if (cropResizing) {
            if (cropResizeHandle === 'br') {
                cropRect.w = Math.max(50, cropRect.w + dx);
                cropRect.h = Math.max(50, cropRect.h + dy);
            } else if (cropResizeHandle === 'bl') {
                cropRect.x += dx; cropRect.w = Math.max(50, cropRect.w - dx);
                cropRect.h = Math.max(50, cropRect.h + dy);
            } else if (cropResizeHandle === 'tr') {
                cropRect.w = Math.max(50, cropRect.w + dx);
                cropRect.y += dy; cropRect.h = Math.max(50, cropRect.h - dy);
            } else if (cropResizeHandle === 'tl') {
                cropRect.x += dx; cropRect.w = Math.max(50, cropRect.w - dx);
                cropRect.y += dy; cropRect.h = Math.max(50, cropRect.h - dy);
            }
            cropDragStart = { x: mx, y: my };
        }

        // Send crop update to server
        if (cropDragging || cropResizing) {
            sendCropUpdate();
        }
    });

    canvas.addEventListener('mouseup', () => {
        cropDragging = false;
        cropResizing = false;
        cropResizeHandle = null;
    });
}

function sendCropUpdate() {
    // Send crop rect to server via SSE or fetch
    fetch('/api/v1/camera/crop', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(cropRect)
    }).catch(() => {});
}

const SKELETON_BONES = [
    [0,1],[0,2],[1,3],[2,4],
    [5,6],
    [5,7],[7,9],
    [6,8],[8,10],
    [5,11],[6,12],
    [11,12],
    [11,13],[13,15],
    [12,14],[14,16]
];

function drawCropBoundary(ctx, w, h) {
    // Scale crop rect to canvas size
    const sx = w / 640, sy = h / 360;
    const cx = cropRect.x * sx, cy = cropRect.y * sy;
    const cw = cropRect.w * sx, ch = cropRect.h * sy;

    // Dashed boundary
    ctx.strokeStyle = '#fbbf24';
    ctx.lineWidth = 1.5;
    ctx.setLineDash([6, 4]);
    ctx.strokeRect(cx, cy, cw, ch);
    ctx.setLineDash([]);

    // Corner handles
    const hs = 6;
    ctx.fillStyle = '#fbbf24';
    ctx.fillRect(cx - hs/2, cy - hs/2, hs, hs);           // tl
    ctx.fillRect(cx + cw - hs/2, cy - hs/2, hs, hs);       // tr
    ctx.fillRect(cx - hs/2, cy + ch - hs/2, hs, hs);       // bl
    ctx.fillRect(cx + cw - hs/2, cy + ch - hs/2, hs, hs);  // br

    // Label
    ctx.fillStyle = '#fbbf24';
    ctx.font = '10px monospace';
    ctx.fillText(`crop: ${Math.round(cropRect.x)},${Math.round(cropRect.y)} ${Math.round(cropRect.w)}x${Math.round(cropRect.h)}`, cx + 2, cy - 4);
}

function drawSkeletonOverlay(ctx, keypoints, w, h) {
    if (!keypoints || keypoints.length < 17) return;

    // Draw bones
    ctx.lineWidth = 3;
    ctx.strokeStyle = '#fbbf24';
    for (const [i, j] of SKELETON_BONES) {
        const a = keypoints[i], b = keypoints[j];
        if (a.confidence > 0.3 && b.confidence > 0.3) {
            ctx.beginPath();
            ctx.moveTo(a.x * w / 640, a.y * h / 360);
            ctx.lineTo(b.x * w / 640, b.y * h / 360);
            ctx.stroke();
        }
    }

    // Draw keypoints
    for (let i = 0; i < keypoints.length; i++) {
        const kp = keypoints[i];
        if (kp.confidence > 0.3) {
            ctx.beginPath();
            ctx.arc(kp.x * w / 640, kp.y * h / 360, 4, 0, Math.PI * 2);
            ctx.fillStyle = (i === 5 || i === 7 || i === 9 || i === 11 || i === 13 || i === 15)
                ? '#4ade80' : '#f87171';
            ctx.fill();
        }
    }
}

function initLiveView() {
    const canvas = document.getElementById('video-canvas');
    const placeholder = document.getElementById('video-placeholder');
    const ctx = canvas.getContext('2d');
    let frameImg = new Image();

    frameImg.onload = () => {
        if (canvas.width !== frameImg.width) canvas.width = frameImg.width;
        if (canvas.height !== frameImg.height) canvas.height = frameImg.height;
        ctx.drawImage(frameImg, 0, 0);
        // Draw skeleton overlay on video
        drawSkeletonOverlay(ctx, latestKeypoints, canvas.width, canvas.height);
        if (placeholder) placeholder.style.display = 'none';
    };

    // Populate angles display
    const anglesDiv = document.getElementById('angles-display');
    if (anglesDiv) {
        let html = '';
        for (const mv of MOVEMENTS) {
            const label = mv.replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase());
            html += `<div class="angle-row">
                <span class="label">${label} L/R</span>
                <span>
                    <span class="value left" id="angle-${mv}-left">—°</span>
                    <span style="color:var(--text-secondary)"> / </span>
                    <span class="value right" id="angle-${mv}-right">—°</span>
                </span>
            </div>`;
        }
        anglesDiv.innerHTML = html;
    }

    // Mode toggle
    const modeLive = document.getElementById('mode-live');
    const modeCompare = document.getElementById('mode-compare');
    const panelLive = document.getElementById('panel-live');
    const panelCompare = document.getElementById('panel-compare');

    modeLive?.addEventListener('click', () => {
        modeLive.classList.add('active');
        modeCompare?.classList.remove('active');
        if (panelLive) panelLive.style.display = '';
        if (panelCompare) panelCompare.style.display = 'none';
    });

    modeCompare?.addEventListener('click', () => {
        modeCompare.classList.add('active');
        modeLive?.classList.remove('active');
        if (panelLive) panelLive.style.display = 'none';
        if (panelCompare) panelCompare.style.display = '';
    });

    // Side selector
    const btnLeft = document.getElementById('btn-side-left');
    const btnRight = document.getElementById('btn-side-right');
    btnLeft?.addEventListener('click', () => {
        selectedSide = 'left';
        btnLeft.classList.add('active');
        btnRight?.classList.remove('active');
    });
    btnRight?.addEventListener('click', () => {
        selectedSide = 'right';
        btnRight.classList.add('active');
        btnLeft?.classList.remove('active');
    });

    // Capture A
    document.getElementById('btn-capture-a')?.addEventListener('click', () => {
        capturedFrameA = {
            keypoints: latestKeypoints ? JSON.parse(JSON.stringify(latestKeypoints)) : null,
            angles: JSON.parse(JSON.stringify(latestAngles)),
            time: new Date().toLocaleTimeString()
        };
        document.getElementById('compare-status').innerHTML =
            `<span style="color:var(--accent-green)">✓ A captured (${capturedFrameA.time})</span> — Move arm, then capture B`;
    });

    // Capture B
    document.getElementById('btn-capture-b')?.addEventListener('click', () => {
        capturedFrameB = {
            keypoints: latestKeypoints ? JSON.parse(JSON.stringify(latestKeypoints)) : null,
            angles: JSON.parse(JSON.stringify(latestAngles)),
            time: new Date().toLocaleTimeString()
        };
        document.getElementById('compare-status').innerHTML =
            `<span style="color:var(--accent-green)">✓ A + B captured</span> — Click Compare`;
    });

    // Compare
    document.getElementById('btn-compare')?.addEventListener('click', () => {
        if (!capturedFrameA || !capturedFrameB) {
            document.getElementById('compare-status').innerHTML = '<span style="color:var(--accent-red)">Capture both frames first</span>';
            return;
        }
        comparisonActive = true;
        showComparison(capturedFrameA, capturedFrameB, selectedSide);
        if (skeleton3d) updateSkeleton3DComparison(capturedFrameA.keypoints, capturedFrameB.keypoints);
        document.getElementById('legend-a').style.display = '';
        document.getElementById('legend-b').style.display = '';
        document.getElementById('legend-arrow').style.display = '';
    });

    // Reset
    document.getElementById('btn-reset')?.addEventListener('click', () => {
        capturedFrameA = null;
        capturedFrameB = null;
        comparisonActive = false;
        document.getElementById('compare-result').innerHTML = '';
        document.getElementById('compare-status').innerHTML = 'Select side → Capture A → Move → Capture B → Compare';
        document.getElementById('legend-a').style.display = 'none';
        document.getElementById('legend-b').style.display = 'none';
        document.getElementById('legend-arrow').style.display = 'none';
        if (skeleton3d) {
            skeleton3d.skeletonGroup.children.filter(c => c.userData?.comp).forEach(c => skeleton3d.skeletonGroup.remove(c));
        }
    });

    // Init 3D skeleton
    initSkeleton3D();

    // ROM angles chart
    realtimeAngleChart = ChartUtils.createRealtimeChart('angle-chart', {
        title: 'Abduction L/R',
        yLabel: 'Angle (°)',
        maxPoints: 120,
    });



    // Capture buttons
    document.getElementById('btn-capture-a')?.addEventListener('click', () => {
        capturedFrameA = {
            keypoints: latestKeypoints ? JSON.parse(JSON.stringify(latestKeypoints)) : null,
            angles: JSON.parse(JSON.stringify(latestAngles)),
            time: new Date().toLocaleTimeString()
        };
        document.getElementById('compare-status').innerHTML =
            `<span style="color:var(--accent-green)">A captured (${capturedFrameA.time})</span> — Move arm to max ROM, then capture B`;
        showToast('Frame A captured (resting)');
    });

    document.getElementById('btn-capture-b')?.addEventListener('click', () => {
        capturedFrameB = {
            keypoints: latestKeypoints ? JSON.parse(JSON.stringify(latestKeypoints)) : null,
            angles: JSON.parse(JSON.stringify(latestAngles)),
            time: new Date().toLocaleTimeString()
        };
        document.getElementById('compare-status').innerHTML =
            `<span style="color:var(--accent-green)">A (${capturedFrameA?.time || '—'}) + B (${capturedFrameB.time}) captured</span> — Click Compare`;
        showToast('Frame B captured (max ROM)');
    });

    // Compare button
    document.getElementById('btn-compare')?.addEventListener('click', () => {
        if (!capturedFrameA || !capturedFrameB) {
            showToast('Capture both frames first');
            return;
        }
        comparisonActive = true;
        showComparison(capturedFrameA, capturedFrameB, selectedSide);
        // Update 3D skeleton to show both frames
        if (skeleton3d) {
            updateSkeleton3DComparison(capturedFrameA.keypoints, capturedFrameB.keypoints);
        }
    });

    // Reset button
    document.getElementById('btn-reset')?.addEventListener('click', () => {
        capturedFrameA = null;
        capturedFrameB = null;
        comparisonActive = false;
        document.getElementById('compare-result').innerHTML = '';
        document.getElementById('compare-status').innerHTML =
            'Select side → Capture A (resting) → Move arm → Capture B (max) → Compare';
        // Reset 3D skeleton to live
        if (skeleton3d) {
            skeleton3d.skeletonGroup.children.forEach(c => {
                if (c.userData?.frameA) c.visible = false;
                if (c.userData?.frameB) c.visible = false;
            });
        }
        showToast('Reset complete');
    });

    // Update chart title when movement selection changes
    const chartSel = document.getElementById('chart-movement');
    if (chartSel) {
        chartSel.addEventListener('change', () => {
            const label = chartSel.options[chartSel.selectedIndex].text;
            if (realtimeAngleChart && realtimeAngleChart.options && realtimeAngleChart.options.plugins) {
                realtimeAngleChart.options.plugins.title.text = label + ' L/R';
                realtimeAngleChart.update();
            }
            // Clear chart data
            realtimeAngleChart.data.labels = [];
            realtimeAngleChart.data.datasets[0].data = [];
            realtimeAngleChart.data.datasets[1].data = [];
            realtimeAngleChart.update();
        });
    }

    const ws = new OculusWebSocket();
    ws.on('connected', () => {
        updateConnectionStatus(true);
        showToast('Connected');
    });
    ws.on('disconnected', () => {
        updateConnectionStatus(false);
    });
    ws.on('rom_angles', (data) => {
        updateLiveAngles(data);
        latestAngles = data;
    });
    ws.on('video_frame', (data) => {
        if (data.frame) {
            frameImg.src = 'data:image/jpeg;base64,' + data.frame;
        }
    });
    ws.on('keypoints', (data) => {
        if (data.keypoints) {
            latestKeypoints = data.keypoints;
            updateSkeleton3D(data.keypoints);
        }
    });
    ws.on('data', (data) => {
        if (data.angles) updateLiveAngles(data);
        if (data.frame) frameImg.src = 'data:image/jpeg;base64,' + data.frame;
        if (data.keypoints) {
            latestKeypoints = data.keypoints;
            updateSkeleton3D(data.keypoints);
        }
    });
    ws.connect();
}

function showComparison(a, b, side) {
    const flatten = (data) => {
        const flat = {};
        const angles = data.angles?.angles || data.angles || {};
        for (const [key, val] of Object.entries(angles)) {
            if (typeof val === 'object' && val !== null) {
                for (const [sub, v] of Object.entries(val)) {
                    flat[`${key}_${sub}`] = v;
                }
            }
        }
        return flat;
    };

    const fa = flatten(a);
    const fb = flatten(b);

    let html = `<div style="font-size:0.75rem;margin-bottom:0.5rem">`;
    html += `<strong>${side.toUpperCase()} ARM</strong> — A: ${a.time} vs B: ${b.time}</div>`;
    html += `<div style="display:grid;grid-template-columns:1fr auto 1fr;gap:2px;font-size:0.8rem">`;
    html += `<div style="font-weight:600;color:var(--accent)">A (Rest)</div>`;
    html += `<div style="font-weight:600">Diff</div>`;
    html += `<div style="font-weight:600;color:var(--accent-orange)">B (Max)</div>`;

    for (const mv of MOVEMENTS) {
        const valA = fa[`${side}_${mv}`] ?? 0;
        const valB = fb[`${side}_${mv}`] ?? 0;
        const diff = valB - valA;
        const color = Math.abs(diff) > 30 ? 'var(--accent-red)' :
                      Math.abs(diff) > 10 ? 'var(--accent-orange)' :
                      'var(--accent-green)';

        html += `<div>${valA.toFixed(1)}°</div>`;
        html += `<div style="color:${color};font-weight:600">${diff > 0 ? '+' : ''}${diff.toFixed(1)}°</div>`;
        html += `<div>${valB.toFixed(1)}°</div>`;
    }

    html += `</div>`;
    document.getElementById('compare-result').innerHTML = html;
}

function updateLiveAngles(data) {
    const angles = data.angles || data;
    const time = new Date().toLocaleTimeString();

    // Flatten nested server format: { left: { abduction: 179 }, left_forward: { flexion: 180 } }
    // to: { left_abduction: 179, left_forward_flexion: 180 }
    const flat = {};
    for (const [key, val] of Object.entries(angles)) {
        if (typeof val === 'object' && val !== null) {
            for (const [sub, v] of Object.entries(val)) {
                flat[`${key}_${sub}`] = v;
            }
        }
    }

    // Update text displays
    for (const movement of MOVEMENTS) {
        const left = flat[`left_${movement}`] ?? flat[`left_${movement.replace('_', '_')}`];
        const right = flat[`right_${movement}`] ?? flat[`right_${movement.replace('_', '_')}`];

        const leftEl = document.getElementById(`angle-${movement}-left`);
        const rightEl = document.getElementById(`angle-${movement}-right`);
        if (leftEl && left !== undefined) leftEl.textContent = left.toFixed(1) + '°';
        if (rightEl && right !== undefined) rightEl.textContent = right.toFixed(1) + '°';
    }

    // Update chart — use selected movement
    const sel = document.getElementById('chart-movement');
    const mv = sel ? sel.value : 'abduction';
    const chartLeft = flat[`left_${mv}`] ?? 0;
    const chartRight = flat[`right_${mv}`] ?? 0;
    ChartUtils.pushChartData(realtimeAngleChart, time, [chartLeft, chartRight]);
}

/* ── Analysis (analysis.html) ── */

let comparisonChart = null;
let radarChart = null;

async function initAnalysis() {
    const params = new URLSearchParams(window.location.search);
    const sessionId = params.get('session');

    const analysis = await getAnalysis();
    if (!analysis) {
        setText('analysis-status', 'No analysis data available');
        return;
    }

    renderRomTable(analysis);
    renderRomBars(analysis);

    const movements = [];
    const leftRom = [];
    const rightRom = [];

    for (const m of MOVEMENTS) {
        const left = analysis.left?.[m]?.rom ?? 0;
        const right = analysis.right?.[m]?.rom ?? 0;
        if (left > 0 || right > 0) {
            movements.push(m);
            leftRom.push(left);
            rightRom.push(right);
        }
    }

    if (movements.length > 0) {
        comparisonChart = ChartUtils.createComparisonChart('comparison-chart', movements, leftRom, rightRom);
        radarChart = ChartUtils.createRadarChart('radar-chart', movements, leftRom, rightRom);
    }

    const sym = analysis.symmetry_pct ?? 0;
    setText('symmetry-value', sym.toFixed(1) + '%');
    const symEl = document.getElementById('symmetry-value');
    if (symEl) {
        symEl.style.color = sym >= 90 ? 'var(--accent-green)' :
                            sym >= 70 ? 'var(--accent-orange)' : 'var(--accent-red)';
    }

    loadSessionList();
}

function renderRomTable(analysis) {
    const tbody = document.getElementById('rom-tbody');
    if (!tbody) return;

    tbody.innerHTML = MOVEMENTS.map(m => {
        const left = analysis.left?.[m] || {};
        const right = analysis.right?.[m] || {};
        return `<tr>
            <td>${ChartUtils.formatMovementName(m)}</td>
            <td>${fmt(left.min_angle)}°</td>
            <td>${fmt(left.max_angle)}°</td>
            <td style="color:var(--accent);font-weight:600">${fmt(left.rom)}°</td>
            <td>${fmt(right.min_angle)}°</td>
            <td>${fmt(right.max_angle)}°</td>
            <td style="color:var(--accent-orange);font-weight:600">${fmt(right.rom)}°</td>
        </tr>`;
    }).join('');
}

function renderRomBars(analysis) {
    const container = document.getElementById('rom-bars');
    if (!container) return;

    let maxRom = 0;
    for (const m of MOVEMENTS) {
        const l = analysis.left?.[m]?.rom ?? 0;
        const r = analysis.right?.[m]?.rom ?? 0;
        maxRom = Math.max(maxRom, l, r);
    }
    if (maxRom === 0) maxRom = 1;

    container.innerHTML = MOVEMENTS.map(m => {
        const left = analysis.left?.[m]?.rom ?? 0;
        const right = analysis.right?.[m]?.rom ?? 0;
        const lw = (left / maxRom * 100).toFixed(1);
        const rw = (right / maxRom * 100).toFixed(1);
        return `<div class="rom-bar-container">
            <div class="rom-bar-label">
                <span class="movement-name">${ChartUtils.formatMovementName(m)}</span>
                <span class="rom-value">L: ${left.toFixed(1)}° / R: ${right.toFixed(1)}°</span>
            </div>
            <div class="rom-bar">
                <div class="bar-left" style="width:${lw}%"></div>
                <div class="bar-right" style="width:${rw}%"></div>
            </div>
        </div>`;
    }).join('');
}

/* ── Utilities ── */

function setText(id, text) {
    const el = document.getElementById(id);
    if (el) el.textContent = text;
}

function fmt(val) {
    if (val === undefined || val === null || val >= 900) return '—';
    return val.toFixed(1);
}

function formatUptime(seconds) {
    if (!seconds) return '0s';
    const h = Math.floor(seconds / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    const s = Math.floor(seconds % 60);
    if (h > 0) return `${h}h ${m}m`;
    if (m > 0) return `${m}m ${s}s`;
    return `${s}s`;
}

function formatTimestamp(ts) {
    if (!ts) return '—';
    return new Date(ts * 1000).toLocaleString();
}

function escapeHtml(str) {
    const div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
}