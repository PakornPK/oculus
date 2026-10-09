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

    // Bone connections - shoulder ROM relevant only
    const bones = [
        [5,6],      // shoulders
        [5,7],[7,9],    // L arm
        [6,8],[8,10],   // R arm
        [5,11],[6,12],  // torso
        [11,12],        // hips
    ];

    const lineMatLeft = new THREE.LineBasicMaterial({ color: 0x4ade80 });
    const lineMatRight = new THREE.LineBasicMaterial({ color: 0xf87171 });
    const lineMatCenter = new THREE.LineBasicMaterial({ color: 0xfbbf24 });

    const lines = [];
    for (const [i, j] of bones) {
        const geometry = new THREE.BufferGeometry();
        const isLeft = LEFT_INDICES.has(i) || LEFT_INDICES.has(j);
        const isRight = RIGHT_INDICES.has(i) || RIGHT_INDICES.has(j);
        const mat = (isLeft && !isRight) ? lineMatLeft :
                    (isRight && !isLeft) ? lineMatRight : lineMatCenter;
        const line = new THREE.Line(geometry, mat);
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

// Dedicated comparison 3D scene
let compare3d = null;

function initCompare3D() {
    const container = document.getElementById('compare3d-container');
    if (!container) return;

    const canvas = document.getElementById('compare3d-canvas');
    // Use default size if container is hidden
    const width = container.clientWidth || 400;
    const height = container.clientHeight || 400;

    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x0f172a);

    const camera = new THREE.PerspectiveCamera(60, width / height, 0.1, 1000);
    camera.position.set(0, 1.2, 2.5);
    camera.lookAt(0, 1, 0);

    const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
    renderer.setSize(width, height);

    const controls = new THREE.OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;
    controls.dampingFactor = 0.05;
    controls.target.set(0, 1, 0);

    const grid = new THREE.GridHelper(3, 15, 0x475569, 0x1e293b);
    scene.add(grid);

    const group = new THREE.Group();
    scene.add(group);

    // Materials for A (green) and B (red) skeletons
    const matA = new THREE.MeshBasicMaterial({ color: 0x4ade80 });
    const matB = new THREE.MeshBasicMaterial({ color: 0xf87171 });
    const geo = new THREE.SphereGeometry(0.015, 12, 12);
    const lineMatA = new THREE.LineBasicMaterial({ color: 0x4ade80, transparent: true, opacity: 0.8 });
    const lineMatB = new THREE.LineBasicMaterial({ color: 0xf87171, transparent: true, opacity: 0.8 });
    const arrowMat = new THREE.LineBasicMaterial({ color: 0xfbbf24 });
    const arcMat = new THREE.LineBasicMaterial({ color: 0xfbbf24, transparent: true, opacity: 0.6 });

    const bones = [
        [5,6],[5,7],[7,9],[6,8],[8,10],
        [5,11],[6,12],[11,12]
    ];

    function animate() {
        requestAnimationFrame(animate);
        controls.update();
        renderer.render(scene, camera);
    }
    animate();

    compare3d = { scene, camera, renderer, group, controls, geo, matA, matB, lineMatA, lineMatB, arrowMat, arcMat, bones, width, height };
}

function updateCompare3D(kpA, kpB) {
    if (!compare3d || !kpA || !kpB) return;

    const { group, geo, matA, matB } = compare3d;
    const scale = 0.003;

    while (group.children.length > 0) group.remove(group.children[0]);

    const si = selectedSide === 'right' ? 6 : 5;
    const ei = selectedSide === 'right' ? 8 : 7;
    const wi = selectedSide === 'right' ? 10 : 9;

    // Anchor to Frame A's shoulder position
    const anchorS = kpA[si];
    const anchorOffset = anchorS ? {
        x: -anchorS.x * scale,
        y: anchorS.y * scale
    } : { x: 0, y: 0 };

    const drawArm = (kp, mat, offsetX, offsetY) => {
        const s = kp[si], e = kp[ei], w = kp[wi];
        if (!s || !e || !w || s.confidence < 0.3) return null;

        // Anchor to Frame A's shoulder, both arms at same relative position
        const shoulder = new THREE.Vector3(offsetX, offsetY, 0);
        const elbow = new THREE.Vector3(
            (e.x - s.x) * scale + offsetX,
            -(e.y - s.y) * scale + offsetY,
            0
        );
        const wrist = new THREE.Vector3(
            (w.x - s.x) * scale + offsetX,
            -(w.y - s.y) * scale + offsetY,
            0
        );

        // Keypoints
        for (const p of [shoulder, elbow, wrist]) {
            const sp = new THREE.Mesh(geo, mat);
            sp.position.copy(p);
            group.add(sp);
        }

        // Bones
        const boneMat = new THREE.LineBasicMaterial({ color: mat.color.getHex() });
        for (const [a, b] of [[shoulder, elbow], [elbow, wrist]]) {
            group.add(new THREE.Line(
                new THREE.BufferGeometry().setFromPoints([a, b]),
                boneMat
            ));
        }

        // Calculate angle using same method as server (torso vertical reference)
        const v1 = elbow.clone().sub(shoulder);
        const v2 = new THREE.Vector3(0, -1, 0);  // vertical down
        const dot = v1.dot(v2);
        const mag = v1.length();
        const cosA = mag > 0 ? Math.max(-1, Math.min(1, dot / mag)) : 0;
        const angle = Math.acos(cosA) * 180 / Math.PI;

        // Draw arc
        const arcMat = new THREE.LineBasicMaterial({ color: 0xfbbf24 });
        const arcPts = [];
        for (let i = 0; i <= 20; i++) {
            const t = i / 20;
            const a = -Math.PI / 2 + (Math.atan2(v1.y, v1.x) + Math.PI / 2) * t;
            arcPts.push(new THREE.Vector3(Math.cos(a) * 0.15, Math.sin(a) * 0.15, 0));
        }
        group.add(new THREE.Line(new THREE.BufferGeometry().setFromPoints(arcPts), arcMat));

        return { shoulder, elbow, wrist, angle };
    };

    const armA = drawArm(kpA, matA, 0, 0);
    const armB = drawArm(kpB, matB, 0, 0);

    if (armA && armB) {
        // Use server-provided angles for display (consistent with measurement frame)
        const fa = flattenAngles(capturedFrameA?.angles);
        const fb = flattenAngles(capturedFrameB?.angles);
        const mv = document.getElementById('measure-movement')?.value || 'forward_flexion';
        const side = selectedSide === 'right' ? 'right' : 'left';
        const valA = fa[`${side}_${mv}`] ?? armA.angle;
        const valB = fb[`${side}_${mv}`] ?? armB.angle;
        const delta = valB - valA;

        const label = document.getElementById('compare3d-rom-label');
        if (label) {
            label.innerHTML =
                `A: ${valA.toFixed(1)}° | B: ${valB.toFixed(1)}° | ROM: ${delta > 0 ? '+' : ''}${delta.toFixed(1)}°`;
        }
    }

    // Show angle values
    const romLabel = document.getElementById('compare3d-rom-label');
    if (romLabel && armA) {
        const delta = armB ? armB.angle - armA.angle : 0;
        romLabel.innerHTML = `
            <div>A: <strong>${armA.angle.toFixed(1)}°</strong></div>
            ${armB ? `<div>B: <strong>${armB.angle.toFixed(1)}°</strong></div>` : ''}
            ${armB ? `<div>ROM: <strong style="color:var(--accent-orange)">${delta > 0 ? '+' : ''}${delta.toFixed(1)}°</strong></div>` : ''}
        `;
    }
}

function swapToCompare3D() {
    const liveContainer = document.getElementById('skeleton3d-container');
    const compareContainer = document.getElementById('compare3d-container');
    const title = document.getElementById('view3d-title');
    if (liveContainer) liveContainer.style.display = 'none';
    if (compareContainer) compareContainer.style.display = '';
    if (title) title.textContent = '3D Comparison';

    // Resize renderer when container becomes visible
    if (compare3d && compareContainer) {
        const w = compareContainer.clientWidth || 400;
        const h = compareContainer.clientHeight || 400;
        compare3d.renderer.setSize(w, h);
        compare3d.camera.aspect = w / h;
        compare3d.camera.updateProjectionMatrix();
    }
}

function swapToLive3D() {
    const liveContainer = document.getElementById('skeleton3d-container');
    const compareContainer = document.getElementById('compare3d-container');
    const title = document.getElementById('view3d-title');
    if (liveContainer) liveContainer.style.display = '';
    if (compareContainer) compareContainer.style.display = 'none';
    if (title) title.textContent = '3D View';
}

function updateSkeleton3D(keypoints) {
    if (!skeleton3d || !keypoints || keypoints.length < 17) return;

    const { spheres, lines } = skeleton3d;
    const scale = 0.003; // pixel to 3D unit
    const centerY = 1.0;

    // Update sphere positions with depth estimation
    // Shoulder width = reference for depth
    const ls = keypoints[5], rs = keypoints[6];
    const shoulderWidth = (ls && rs && ls.confidence > 0.3 && rs.confidence > 0.3)
        ? Math.abs(ls.x - rs.x) : 100;
    const depthScale = shoulderWidth * scale * 0.5;

    const showBoth = selectedSide === 'both';

    for (let i = 0; i < 17; i++) {
        const kp = keypoints[i];
        if (kp.confidence > 0.3 && isSideVisible(i)) {
            // Depth estimation based on body model
            let z = 0;
            const shoulderX = (i % 2 === 1) ? (ls?.x || 280) : (rs?.x || 360);

            if (i === 0) z = depthScale * 0.8;           // nose - forward
            else if (i === 1 || i === 2) z = depthScale * 0.7;  // eyes
            else if (i === 3 || i === 4) z = depthScale * 0.5;  // ears
            else if (i === 5 || i === 6) z = 0;                  // shoulders - center
            else if (i === 7 || i === 8) {                        // elbows
                const lateralDist = Math.abs(kp.x - shoulderX);
                z = lateralDist * scale * 0.4;
            } else if (i === 9 || i === 10) {                     // wrists
                const lateralDist = Math.abs(kp.x - shoulderX);
                z = lateralDist * scale * 0.6;
            } else if (i === 11 || i === 12) z = 0;               // hips - center
            else if (i === 13 || i === 14) z = -depthScale * 0.1; // knees - slightly back
            else if (i === 15 || i === 16) z = -depthScale * 0.2; // ankles - slightly back

            spheres[i].position.set(
                (kp.x - 320) * scale,
                centerY - (kp.y - 180) * scale,
                z
            );
            spheres[i].visible = true;

            // Dim non-selected side
            if (!showBoth) {
                const isLeft = LEFT_INDICES.has(i);
                const isRight = RIGHT_INDICES.has(i);
                const isSelected = (selectedSide === 'left' && isLeft) || (selectedSide === 'right' && isRight);
                spheres[i].material.transparent = !isSelected;
                spheres[i].material.opacity = isSelected ? 1.0 : 0.15;
            } else {
                spheres[i].material.transparent = false;
                spheres[i].material.opacity = 1.0;
            }
        } else {
            spheres[i].visible = false;
        }
    }

    // Update bone lines
    for (const { line, i, j } of lines) {
        const visible = spheres[i]?.visible && spheres[j]?.visible;
        line.visible = visible;
        if (visible) {
            const pos = new Float32Array([
                spheres[i].position.x, spheres[i].position.y, spheres[i].position.z,
                spheres[j].position.x, spheres[j].position.y, spheres[j].position.z,
            ]);
            line.geometry.setAttribute('position', new THREE.BufferAttribute(pos, 3));
            line.geometry.attributes.position.needsUpdate = true;
        }
    }
}

// Skeleton overlay state
let latestKeypoints = null;
let latestAngles = {};
let capturedFrameA = null;  // { keypoints, angles, time }
let capturedFrameB = null;
let selectedSide = 'left';  // 'left', 'right', or 'both'
let comparisonActive = false;
let isLive = true;



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

// Shoulder ROM skeleton - only arms, shoulders, torso
const SKELETON_BONES = [
    [5,6],      // shoulders
    [5,7],      // L upper arm
    [7,9],      // L forearm
    [6,8],      // R upper arm
    [8,10],     // R forearm
    [5,11],     // L torso
    [6,12],     // R torso
    [11,12],    // hips
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

// Keypoint indices by side (COCO-17 format)
// Shoulder ROM relevant keypoints only
// 5=L-shoulder, 6=R-shoulder, 7=L-elbow, 8=R-elbow, 9=L-wrist, 10=R-wrist
// 11=L-hip(torso), 12=R-hip(torso)
const LEFT_INDICES = new Set([5, 7, 9, 11]);
const RIGHT_INDICES = new Set([6, 8, 10, 12]);
const CENTER_INDICES = new Set([]);
const SHOULDER_ROM_INDICES = new Set([5, 6, 7, 8, 9, 10, 11, 12]);

function isSideVisible(idx) {
    // Only show shoulder ROM relevant keypoints
    if (!SHOULDER_ROM_INDICES.has(idx)) return false;
    if (selectedSide === 'both') return true;
    if (selectedSide === 'left') return LEFT_INDICES.has(idx);
    return RIGHT_INDICES.has(idx);
}

function drawSkeletonOverlay(ctx, keypoints, w, h) {
    if (!keypoints || keypoints.length < 17) return;

    const showBoth = selectedSide === 'both';

    // Draw bones (filtered and color-coded by side)
    ctx.lineWidth = 3;
    for (const [i, j] of SKELETON_BONES) {
        if (!isSideVisible(i) && !isSideVisible(j)) continue;
        const a = keypoints[i], b = keypoints[j];
        if (a.confidence <= 0.3 || b.confidence <= 0.3) continue;

        // Color bone by side: left=green, right=red, cross=center amber
        const isLeftBone = LEFT_INDICES.has(i) || LEFT_INDICES.has(j);
        const isRightBone = RIGHT_INDICES.has(i) || RIGHT_INDICES.has(j);
        ctx.strokeStyle = (isLeftBone && !isRightBone) ? '#4ade80' :
                          (isRightBone && !isLeftBone) ? '#f87171' : '#fbbf24';

        // Dim non-selected side when filtering
        if (!showBoth) {
            const isSelected = (selectedSide === 'left' && isLeftBone) ||
                               (selectedSide === 'right' && isRightBone);
            ctx.globalAlpha = isSelected ? 1.0 : 0.15;
        } else {
            ctx.globalAlpha = 1.0;
        }

        ctx.beginPath();
        ctx.moveTo(a.x * w / frameWidth, a.y * h / frameHeight);
        ctx.lineTo(b.x * w / frameWidth, b.y * h / frameHeight);
        ctx.stroke();
    }
    ctx.globalAlpha = 1.0;

    // Draw keypoints (filtered by selected side)
    for (let i = 0; i < keypoints.length; i++) {
        if (!isSideVisible(i)) continue;
        const kp = keypoints[i];
        if (kp.confidence <= 0.3) continue;

        const isLeft = LEFT_INDICES.has(i);
        const isRight = RIGHT_INDICES.has(i);

        if (!showBoth) {
            const isSelected = (selectedSide === 'left' && isLeft) ||
                               (selectedSide === 'right' && isRight);
            ctx.globalAlpha = isSelected ? 1.0 : 0.15;
        } else {
            ctx.globalAlpha = 1.0;
        }

        ctx.beginPath();
        ctx.arc(kp.x * w / frameWidth, kp.y * h / frameHeight, 5, 0, Math.PI * 2);
        ctx.fillStyle = isLeft ? '#4ade80' : isRight ? '#f87171' : '#38bdf8';
        ctx.fill();
    }
    ctx.globalAlpha = 1.0;
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

    // Side selector
    const btnLeft = document.getElementById('btn-side-left');
    const btnRight = document.getElementById('btn-side-right');
    const btnBoth = document.getElementById('btn-side-both');
    const sideButtons = [btnLeft, btnRight, btnBoth];

    function setSide(side) {
        selectedSide = side;
        sideButtons.forEach(b => b?.classList.remove('active'));
        if (side === 'left') btnLeft?.classList.add('active');
        else if (side === 'right') btnRight?.classList.add('active');
        else btnBoth?.classList.add('active');

        // Update video overlay indicator
        const indicator = document.getElementById('side-indicator');
        if (indicator) {
            if (side === 'left') {
                indicator.innerHTML = '<span style="color:#4ade80">●</span> LEFT';
            } else if (side === 'right') {
                indicator.innerHTML = '<span style="color:#f87171">●</span> RIGHT';
            } else {
                indicator.innerHTML = '<span style="color:#38bdf8">●</span> BOTH';
            }
        }
    }

    function updateWorkflow(step) {
        const steps = ['wf-step1', 'wf-step2', 'wf-step3', 'wf-step4'];
        steps.forEach((id, i) => {
            const el = document.getElementById(id);
            if (el) {
                if (i < step) {
                    el.style.color = 'var(--accent-green)';
                    el.textContent = el.textContent.replace('○', '●');
                } else if (i === step) {
                    el.style.color = 'var(--accent)';
                    el.textContent = el.textContent.replace('○', '●');
                } else {
                    el.style.color = 'var(--text-secondary)';
                    el.textContent = el.textContent.replace('●', '○');
                }
            }
        });
    }

    btnLeft?.addEventListener('click', () => { setSide('left'); updateWorkflow(1); });
    btnRight?.addEventListener('click', () => { setSide('right'); updateWorkflow(1); });
    btnBoth?.addEventListener('click', () => { setSide('both'); updateWorkflow(1); });

    // Capture A
    document.getElementById('btn-capture-a')?.addEventListener('click', () => {
        capturedFrameA = {
            keypoints: latestKeypoints ? JSON.parse(JSON.stringify(latestKeypoints)) : null,
            angles: JSON.parse(JSON.stringify(latestAngles)),
            time: new Date().toLocaleTimeString()
        };
        document.getElementById('capture-status').innerHTML =
            `<span style="color:var(--accent-green)">✓ A captured (${capturedFrameA.time})</span>`;
        updateWorkflow(2);
        showToast('Frame A captured (resting)');
    });

    // Capture B
    document.getElementById('btn-capture-b')?.addEventListener('click', () => {
        capturedFrameB = {
            keypoints: latestKeypoints ? JSON.parse(JSON.stringify(latestKeypoints)) : null,
            angles: JSON.parse(JSON.stringify(latestAngles)),
            time: new Date().toLocaleTimeString()
        };
        document.getElementById('capture-status').innerHTML =
            `<span style="color:var(--accent-green)">✓ A (${capturedFrameA?.time || '—'}) + B (${capturedFrameB.time})</span>`;
        updateWorkflow(3);
        showToast('Frame B captured (max ROM)');
    });

    // Measure ROM (Gocator-style: select movement → get ROM)
    document.getElementById('btn-measure')?.addEventListener('click', () => {
        if (!capturedFrameA || !capturedFrameB) {
            document.getElementById('capture-status').innerHTML =
                '<span style="color:var(--accent-red)">Capture both frames first</span>';
            return;
        }

        const movement = document.getElementById('measure-movement')?.value;
        if (!movement) {
            showToast('Select a movement first');
            return;
        }

        const flatten = (data) => {
            const flat = {};
            const angles = data?.angles?.angles || data?.angles || {};
            for (const [key, val] of Object.entries(angles)) {
                if (typeof val === 'object' && val !== null) {
                    for (const [sub, v] of Object.entries(val)) {
                        flat[`${key}_${sub}`] = v;
                    }
                }
            }
            return flat;
        };

        const fa = flatten(capturedFrameA.angles);
        const fb = flatten(capturedFrameB.angles);
        const side = selectedSide;

        const valA = fa[`${side}_${movement}`] ?? 0;
        const valB = fb[`${side}_${movement}`] ?? 0;
        const rom = valB - valA;
        const absROM = Math.abs(rom);

        let severity, color;
        if (absROM < 10) { severity = 'Normal'; color = '#2ECC71'; }
        else if (absROM < 30) { severity = 'Mild'; color = '#F5A623'; }
        else if (absROM < 60) { severity = 'Moderate'; color = '#E74C3C'; }
        else { severity = 'Significant'; color = '#E74C3C'; }

        const label = movement.replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase());

        document.getElementById('measure-result').innerHTML = `
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
            </div>
        `;

        swapToCompare3D();
        updateCompare3D(capturedFrameA.keypoints, capturedFrameB.keypoints);

        showToast(`${label} ROM: ${rom.toFixed(1)}° (${severity})`);
    });

    // Batch: Measure all movements
    document.getElementById('btn-batch')?.addEventListener('click', () => {
        if (!capturedFrameA || !capturedFrameB) {
            showToast('Capture A and B first');
            return;
        }

        const flatten = (data) => {
            const flat = {};
            const angles = data?.angles?.angles || data?.angles || {};
            for (const [key, val] of Object.entries(angles)) {
                if (typeof val === 'object' && val !== null) {
                    for (const [sub, v] of Object.entries(val)) {
                        flat[`${key}_${sub}`] = v;
                    }
                }
            }
            return flat;
        };

        const fa = flatten(capturedFrameA.angles);
        const fb = flatten(capturedFrameB.angles);
        const side = selectedSide;
        const movements = [
            'abduction', 'forward_flexion', 'extension',
            'external_rotation', 'internal_rotation', 'adduction',
            'horizontal_adduction', 'scapular_protraction',
            'scapular_retraction', 'shoulder_elevation', 'shoulder_depression'
        ];

        let html = '<div style="font-size:0.75rem">';
        html += `<div style="font-weight:600;margin-bottom:0.5rem">${side.toUpperCase()} ARM — Batch Measurement</div>`;
        html += '<table style="width:100%;border-collapse:collapse">';
        html += '<tr style="border-bottom:1px solid var(--border)">';
        html += '<td style="padding:3px 0">Movement</td><td style="text-align:right">A</td><td style="text-align:right">ROM</td><td style="text-align:right">B</td></tr>';

        for (const mv of movements) {
            const a = fa[`${side}_${mv}`] ?? 0;
            const b = fb[`${side}_${mv}`] ?? 0;
            const rom = b - a;
            const abs = Math.abs(rom);
            const color = abs < 10 ? '#2ECC71' : abs < 30 ? '#F5A623' : abs < 60 ? '#E74C3C' : '#E74C3C';

            html += `<tr style="border-bottom:1px solid var(--border)">`;
            html += `<td style="padding:3px 0;font-size:0.7rem">${mv.replace(/_/g, ' ')}</td>`;
            html += `<td style="text-align:right;font-size:0.75rem;color:#4FC3F7">${a.toFixed(1)}°</td>`;
            html += `<td style="text-align:right;font-size:0.75rem;font-weight:600;color:${color}">${rom > 0 ? '+' : ''}${rom.toFixed(1)}°</td>`;
            html += `<td style="text-align:right;font-size:0.75rem;color:#FFB74D">${b.toFixed(1)}°</td>`;
            html += `</tr>`;
        }
        html += '</table></div>';

        document.getElementById('measure-result').innerHTML = html;
        document.getElementById('batch-status').innerHTML =
            `<span style="color:var(--accent-green)">✓ Batch complete — 11 movements measured</span>`;
        showToast('Batch measurement complete');
    });

    // Export CSV
    document.getElementById('btn-export-csv')?.addEventListener('click', () => {
        const movements = ['abduction', 'forward_flexion', 'extension', 'external_rotation', 'internal_rotation', 'adduction', 'horizontal_adduction', 'scapular_protraction', 'scapular_retraction', 'shoulder_elevation', 'shoulder_depression'];
        const flatten = (data) => {
            const flat = {};
            const angles = data?.angles?.angles || data?.angles || {};
            for (const [key, val] of Object.entries(angles)) {
                if (typeof val === 'object' && val !== null) {
                    for (const [sub, v] of Object.entries(val)) flat[`${key}_${sub}`] = v;
                }
            }
            return flat;
        };
        const fa = flatten(capturedFrameA?.angles);
        const fb = flatten(capturedFrameB?.angles);

        let csv = 'movement,side,angle_a,angle_b,rom\n';
        for (const mv of movements) {
            const a = fa[`${selectedSide}_${mv}`] ?? 0;
            const b = fb[`${selectedSide}_${mv}`] ?? 0;
            csv += `${mv},${selectedSide},${a.toFixed(1)},${b.toFixed(1)},${(b-a).toFixed(1)}\n`;
        }

        const blob = new Blob([csv], { type: 'text/csv' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = 'rom_measurements.csv';
        a.click();
        showToast('CSV exported');
    });

    // Export JSON
    document.getElementById('btn-export-json')?.addEventListener('click', () => {
        const data = {
            session: new Date().toISOString(),
            side: selectedSide,
            measurements: {
                frameA: capturedFrameA?.angles,
                frameB: capturedFrameB?.angles
            }
        };
        const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = 'rom_measurements.json';
        a.click();
        showToast('JSON exported');
    });

    // Report
    document.getElementById('btn-report')?.addEventListener('click', () => {
        const movements = ['abduction', 'forward_flexion', 'extension', 'external_rotation', 'internal_rotation', 'adduction', 'horizontal_adduction', 'scapular_protraction', 'scapular_retraction', 'shoulder_elevation', 'shoulder_depression'];
        const flatten = (data) => {
            const flat = {};
            const angles = data?.angles?.angles || data?.angles || {};
            for (const [key, val] of Object.entries(angles)) {
                if (typeof val === 'object' && val !== null) {
                    for (const [sub, v] of Object.entries(val)) flat[`${key}_${sub}`] = v;
                }
            }
            return flat;
        };
        const fa = flatten(capturedFrameA?.angles);
        const fb = flatten(capturedFrameB?.angles);

        let html = '<div style="font-size:0.75rem">';
        html += `<div style="font-weight:600;margin-bottom:0.5rem">ROM Assessment Report</div>`;
        html += `<div style="color:var(--text-secondary);margin-bottom:0.5rem">${selectedSide.toUpperCase()} | ${new Date().toLocaleDateString()}</div>`;
        html += '<table style="width:100%;border-collapse:collapse">';
        html += '<tr style="border-bottom:1px solid var(--border)"><td>Movement</td><td style="text-align:right">A</td><td style="text-align:right">ROM</td><td style="text-align:right">B</td><td style="text-align:right">Status</td></tr>';

        for (const mv of movements) {
            const a = fa[`${selectedSide}_${mv}`] ?? 0;
            const b = fb[`${selectedSide}_${mv}`] ?? 0;
            const rom = b - a;
            const abs = Math.abs(rom);
            const status = abs < 10 ? 'Normal' : abs < 30 ? 'Mild' : abs < 60 ? 'Moderate' : 'Severe';
            const color = abs < 10 ? '#2ECC71' : abs < 30 ? '#F5A623' : '#E74C3C';

            html += `<tr style="border-bottom:1px solid var(--border)"><td style="padding:3px 0">${mv.replace(/_/g, ' ')}</td>`;
            html += `<td style="text-align:right">${a.toFixed(1)}°</td>`;
            html += `<td style="text-align:right;font-weight:600;color:${color}">${rom.toFixed(1)}°</td>`;
            html += `<td style="text-align:right">${b.toFixed(1)}°</td>`;
            html += `<td style="text-align:right;color:${color}">${status}</td></tr>`;
        }
        html += '</table></div>';

        document.getElementById('measure-result').innerHTML = html;
        showToast('Report generated');
    });

    // Reset
    document.getElementById('btn-reset')?.addEventListener('click', () => {
        capturedFrameA = null;
        capturedFrameB = null;
        comparisonActive = false;
        document.getElementById('measure-result').innerHTML = '';
        document.getElementById('capture-status').innerHTML = 'Press A at resting, B at max ROM';
        swapToLive3D();
        if (skeleton3d) {
            skeleton3d.skeletonGroup.children.filter(c => c.userData?.comp).forEach(c => skeleton3d.skeletonGroup.remove(c));
        }
        showToast('Reset complete');
    });

    // Start/Stop toggle
    const btnStartStop = document.getElementById('btn-start-stop');
    const liveBadge = document.getElementById('live-badge');
    btnStartStop?.addEventListener('click', () => {
        isLive = !isLive;
        if (isLive) {
            btnStartStop.textContent = '⏸ Stop';
            btnStartStop.style.background = 'var(--accent-green)';
            if (liveBadge) { liveBadge.textContent = 'LIVE'; liveBadge.style.background = ''; }
            showToast('Live resumed');
        } else {
            btnStartStop.textContent = '▶ Start';
            btnStartStop.style.background = 'var(--accent-orange)';
            if (liveBadge) { liveBadge.textContent = 'PAUSED'; liveBadge.style.background = 'var(--accent-orange)'; }
            showToast('Live paused');
        }
    });

    // Keyboard shortcuts for clinical workflow
    document.addEventListener('keydown', (e) => {
        if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;
        if (e.key === 'a' || e.key === 'A') {
            document.getElementById('btn-capture-a')?.click();
        } else if (e.key === 'b' || e.key === 'B') {
            document.getElementById('btn-capture-b')?.click();
        } else if (e.key === 'c' || e.key === 'C') {
            document.getElementById('btn-compare')?.click();
        } else if (e.key === 'r' || e.key === 'R') {
            document.getElementById('btn-reset')?.click();
        } else if (e.key === '1') {
            setSide('left');
        } else if (e.key === '2') {
            setSide('right');
        } else if (e.key === '3') {
            setSide('both');
        }
    });

    // Init 3D skeleton
    initSkeleton3D();
    initCompare3D();

    // ROM angles chart
    realtimeAngleChart = ChartUtils.createRealtimeChart('angle-chart', {
        title: 'Abduction L/R',
        yLabel: 'Angle (°)',
        maxPoints: 120,
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
        if (!isLive) return;
        if (data.frame) {
            frameImg.src = 'data:image/jpeg;base64,' + data.frame;
        }
    });
    ws.on('keypoints', (data) => {
        if (!isLive) return;
        if (data.keypoints) {
            latestKeypoints = data.keypoints;
            if (data.frame_width) frameWidth = data.frame_width;
            if (data.frame_height) frameHeight = data.frame_height;
            updateSkeleton3D(data.keypoints);
        }
    });
    ws.on('rom_angles', (data) => {
        if (!isLive) return;
        updateLiveAngles(data);
        latestAngles = data;
    });
    ws.on('data', (data) => {
        if (!isLive) return;
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

    // ROM severity classification for clinical context
    const romSeverity = (diff) => {
        const abs = Math.abs(diff);
        if (abs > 60) return { label: 'Significant', color: 'var(--accent-red)' };
        if (abs > 30) return { label: 'Moderate', color: 'var(--accent-orange)' };
        if (abs > 10) return { label: 'Mild', color: 'var(--accent)' };
        return { label: 'Normal', color: 'var(--accent-green)' };
    };

    let html = `<div style="font-size:0.75rem;margin-bottom:0.5rem">`;
    html += `<strong>${side.toUpperCase()} ARM</strong> — A: ${a.time} vs B: ${b.time}</div>`;
    html += `<div style="display:grid;grid-template-columns:1fr auto 1fr;gap:2px;font-size:0.8rem">`;
    html += `<div style="font-weight:600;color:var(--accent)">A (Rest)</div>`;
    html += `<div style="font-weight:600">ROM Δ</div>`;
    html += `<div style="font-weight:600;color:var(--accent-orange)">B (Max)</div>`;

    for (const mv of MOVEMENTS) {
        const valA = fa[`${side}_${mv}`] ?? 0;
        const valB = fb[`${side}_${mv}`] ?? 0;
        const diff = valB - valA;
        const sev = romSeverity(diff);

        const label = mv.replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase());
        html += `<div>${valA.toFixed(1)}°</div>`;
        html += `<div style="color:${sev.color};font-weight:600" title="${sev.label}">${diff > 0 ? '+' : ''}${diff.toFixed(1)}°</div>`;
        html += `<div>${valB.toFixed(1)}°</div>`;
    }

    html += `</div>`;

    // ROM severity legend
    html += `<div style="display:flex;gap:0.75rem;margin-top:0.5rem;font-size:0.65rem;color:var(--text-secondary)">`;
    html += `<span><span style="color:var(--accent-green)">●</span> Normal (&lt;10°)</span>`;
    html += `<span><span style="color:var(--accent)">●</span> Mild (10-30°)</span>`;
    html += `<span><span style="color:var(--accent-orange)">●</span> Moderate (30-60°)</span>`;
    html += `<span><span style="color:var(--accent-red)">●</span> Significant (&gt;60°)</span>`;
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

    // Update text displays — emphasize selected side
    for (const movement of MOVEMENTS) {
        const left = flat[`left_${movement}`] ?? flat[`left_${movement.replace('_', '_')}`];
        const right = flat[`right_${movement}`] ?? flat[`right_${movement.replace('_', '_')}`];

        const leftEl = document.getElementById(`angle-${movement}-left`);
        const rightEl = document.getElementById(`angle-${movement}-right`);
        if (leftEl && left !== undefined) {
            leftEl.textContent = left.toFixed(1) + '°';
            leftEl.style.opacity = (selectedSide === 'both' || selectedSide === 'left') ? '1' : '0.4';
        }
        if (rightEl && right !== undefined) {
            rightEl.textContent = right.toFixed(1) + '°';
            rightEl.style.opacity = (selectedSide === 'both' || selectedSide === 'right') ? '1' : '0.4';
        }
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