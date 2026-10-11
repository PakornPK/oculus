#!/usr/bin/env python3
"""MediaPipe Pose worker subprocess.

Protocol:
  stdin:  [4B width][4B height][width*height*3B RGB] repeated
  stdout: JSON line per frame: {"keypoints": [[x,y,conf], ...], "ts": N}

Stays alive until stdin closes or 'quit' command.
"""

import sys
import struct
import json
import numpy as np

def main():
    # Lazy imports to keep startup fast
    import onnxruntime as ort
    from mediapipe.tasks.python import vision as mp_vision
    from mediapipe.tasks.python.core import base_options as mp_base
    import mediapipe as mp
    from pathlib import Path
    from collections import deque

    # Resolve model paths relative to project root
    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parent
    models_dir = project_root / "models"

    # Init YOLOX
    yolox_path = models_dir / "yolox-s.onnx"
    det_sess = ort.InferenceSession(
        str(yolox_path),
        providers=["CPUExecutionProvider"],
    )

    # Init MediaPipe
    mp_model_path = models_dir / "pose_landmarker_lite.task"  # lite for speed
    if not mp_model_path.exists():
        mp_model_path = models_dir / "pose_landmarker_heavy.task"  # fallback
    # Auto-select best delegate per platform:
    #   macOS: GPU (Metal) with RGBA input
    #   Jetson/Linux: GPU (OpenGL/Vulkan) with RGBA, fallback to CPU
    use_rgba = False
    gpu_ok = False

    # Try GPU first
    try:
        base_opts = mp_base.BaseOptions(
            model_asset_path=str(mp_model_path),
            delegate=mp_base.BaseOptions.Delegate.GPU,
        )
        _test_opts = mp_vision.PoseLandmarkerOptions(
            base_options=base_opts, running_mode=mp_vision.RunningMode.VIDEO,
            min_pose_detection_confidence=0.3, min_pose_presence_confidence=0.3,
            min_tracking_confidence=0.3, num_poses=1)
        use_rgba = True  # GPU requires RGBA on macOS
        gpu_ok = True
        print("GPU delegate enabled", file=sys.stderr, flush=True)
    except Exception as e:
        print(f"GPU delegate unavailable ({e}), falling back to CPU", file=sys.stderr, flush=True)

    # Fallback to CPU
    if not gpu_ok:
        base_opts = mp_base.BaseOptions(model_asset_path=str(mp_model_path))
        use_rgba = False
        print("CPU delegate", file=sys.stderr, flush=True)

    opts = mp_vision.PoseLandmarkerOptions(
        base_options=base_opts,
        running_mode=mp_vision.RunningMode.VIDEO,
        min_pose_detection_confidence=0.3,
        min_pose_presence_confidence=0.3,
        min_tracking_confidence=0.3,
        num_poses=1,
    )
    landmarker = mp_vision.PoseLandmarker.create_from_options(opts)

    # MediaPipe 33 → COCO 17 mapping
    MP_TO_COCO = {
        0: 0,   # nose
        11: 5,  # L shoulder
        12: 6,  # R shoulder
        13: 7,  # L elbow
        14: 8,  # R elbow
        15: 9,  # L wrist
        16: 10, # R wrist
        23: 11, # L hip
        24: 12, # R hip
        25: 13, # L knee
        26: 14, # R knee
        27: 15, # L ankle
        28: 16, # R ankle
    }

    # Angle smoothing
    ANGLE_WINDOW = 7
    angle_buffers = {}

    # Mode: 'standalone' (YOLOX+MP) or 'crop' (C++ sends pre-cropped frames)
    mode = 'standalone'

    # State for standalone mode (YOLOX detection)
    prev_box = None
    detect_counter = 0
    DETECT_INTERVAL = 10
    BOX_EMA_ALPHA = 0.6

    def detect_person(rgb_np, w, h):
        """YOLOX person detection. Returns (x1,y1,x2,y2) or None."""
        bgr = rgb_np[:, :, ::-1].copy()
        scale = min(640.0 / w, 640.0 / h)
        nw, nh = int(w * scale), int(h * scale)
        resized = cv2_resize(bgr, nw, nh)
        canvas = np.full((640, 640, 3), 114, dtype=np.uint8)
        dx, dy = (640 - nw) // 2, (640 - nh) // 2
        canvas[dy:dy + nh, dx:dx + nw] = resized
        blob = canvas.astype(np.float32).transpose(2, 0, 1)[np.newaxis]
        dets_out, labels_out = det_sess.run(None, {"input": blob})
        dets, labels = dets_out[0], labels_out[0]
        best, best_score = None, 0.0
        for i, det in enumerate(dets):
            if det[4] > best_score and labels[i] == 0:
                best_score, best = det[4], det
        if best is None or best_score < 0.3:
            return None
        return (
            max(0, int((best[0] - dx) / scale)),
            max(0, int((best[1] - dy) / scale)),
            min(w, int((best[2] - dx) / scale)),
            min(h, int((best[3] - dy) / scale)),
        )

    def cv2_resize(img, w, h):
        """Resize without importing cv2 (use numpy for speed)."""
        # Use PIL or just cv2
        import cv2
        return cv2.resize(img, (w, h))

    def smooth_angle(key, value):
        if key not in angle_buffers:
            angle_buffers[key] = deque(maxlen=ANGLE_WINDOW)
        angle_buffers[key].append(value)
        return sum(angle_buffers[key]) / len(angle_buffers[key])

    def process_frame(rgb_np, w, h, timestamp_ms):
        """Process a single frame. Returns keypoints dict."""
        nonlocal prev_box, detect_counter

        detect_counter += 1
        if detect_counter >= DETECT_INTERVAL or prev_box is None:
            new_box = detect_person(rgb_np, w, h)
            if new_box is not None:
                if prev_box is not None:
                    prev_box = tuple(
                        int(prev_box[i] * BOX_EMA_ALPHA + new_box[i] * (1 - BOX_EMA_ALPHA))
                        for i in range(4)
                    )
                else:
                    prev_box = new_box
            detect_counter = 0

        box = prev_box
        if box is None:
            return {"keypoints": [[0, 0, 0]] * 17, "ts": timestamp_ms}

        x1, y1, x2, y2 = box
        bw, bh = x2 - x1, y2 - y1
        ex, ey = int(bw * 0.1), int(bh * 0.1)
        cx1 = max(0, x1 - ex)
        cy1 = max(0, y1 - ey)
        cx2 = min(w, x2 + ex)
        cy2 = min(h, y2 + ey)
        cw, ch = cx2 - cx1, cy2 - cy1

        if cw < 10 or ch < 10:
            return {"keypoints": [[0, 0, 0]] * 17, "ts": timestamp_ms}

        crop = rgb_np[cy1:cy2, cx1:cx2].copy()  # must be contiguous for MediaPipe
        if use_rgba:
            import cv2
            crop = cv2.cvtColor(crop, cv2.COLOR_RGB2RGBA)
            mp_img = mp.Image(image_format=mp.ImageFormat.SRGBA, data=crop)
        else:
            mp_img = mp.Image(image_format=mp.ImageFormat.SRGB, data=crop)
        result = landmarker.detect_for_video(mp_img, timestamp_ms)

        kps = [[0, 0, 0]] * 17

        if result.pose_landmarks:
            lm = result.pose_landmarks[0]
            for mp_idx, coco_idx in MP_TO_COCO.items():
                l = lm[mp_idx]
                kps[coco_idx] = [
                    l.x * cw + cx1,
                    l.y * ch + cy1,
                    l.visibility,
                ]

        return {"keypoints": kps, "ts": timestamp_ms}

    # Signal ready
    print("ready", flush=True)

    # Main loop: read frames from stdin
    stdin = sys.stdin.buffer
    frame_count = 0

    while True:
        # Read header: 8 or 16 bytes
        # 8-byte:  [width:4][height:4] → standalone mode (YOLOX + MP)
        # 16-byte: [crop_w:4][crop_h:4][offset_x:4][offset_y:4] → crop mode
        header = stdin.read(8)
        if not header or len(header) < 8:
            break

        width, height = struct.unpack("<II", header[:8])
        if width == 0 or height == 0 or width > 10000 or height > 10000:
            break

        offset_x, offset_y = 0, 0

        # Check if this is crop mode (16-byte header)
        # Heuristic: if data_size would be too large for a crop, read more header
        # Actually, use a separate protocol byte. Let's check the second 4 bytes:
        # For crop mode, C++ sends 16 bytes. We already read 8. Read 8 more.
        # Detect mode: C++ sets width to (crop_w | 0x80000000) to signal crop mode
        if width > 0x7FFFFFFF:
            # Crop mode: width has high bit set
            width = width & 0x7FFFFFFF
            extra = stdin.read(8)
            if not extra or len(extra) < 8:
                break
            offset_x, offset_y = struct.unpack("<II", extra)
            mode = 'crop'

        data_size = width * height * 3
        data = stdin.read(data_size)
        if not data or len(data) < data_size:
            break

        rgb_np = np.frombuffer(data, dtype=np.uint8).reshape((height, width, 3))
        timestamp_ms = int(frame_count * 40)

        if mode == 'crop':
            # Pre-cropped frame: run MediaPipe directly, map coords back to original
            if rgb_np.shape[0] < 10 or rgb_np.shape[1] < 10:
                result = {"keypoints": [[0, 0, 0]] * 17, "ts": timestamp_ms}
            else:
                crop_img = rgb_np.copy()  # ensure contiguous
                if use_rgba:
                    import cv2
                    crop_img = cv2.cvtColor(crop_img, cv2.COLOR_RGB2RGBA)
                    mp_img = mp.Image(image_format=mp.ImageFormat.SRGBA, data=crop_img)
                else:
                    mp_img = mp.Image(image_format=mp.ImageFormat.SRGB, data=crop_img)
                mp_result = landmarker.detect_for_video(mp_img, timestamp_ms)
                kps = [[0, 0, 0]] * 17
                if mp_result.pose_landmarks:
                    lm = mp_result.pose_landmarks[0]
                    for mp_idx, coco_idx in MP_TO_COCO.items():
                        l = lm[mp_idx]
                        kps[coco_idx] = [
                            l.x * width + offset_x,
                            l.y * height + offset_y,
                            l.visibility,
                        ]
                result = {"keypoints": kps, "ts": timestamp_ms}
        else:
            # Standalone mode: YOLOX + MediaPipe
            result = process_frame(rgb_np, width, height, timestamp_ms)

        print(json.dumps(result), flush=True)
        frame_count += 1

    # Cleanup
    landmarker.close()


if __name__ == "__main__":
    main()