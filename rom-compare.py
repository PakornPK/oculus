#!/usr/bin/env python3
"""
rom-compare.py — 2×2 comparison of shoulder ROM estimation approaches.

Layout:
┌───────────────────┬───────────────────┐
│  RTMPose 2D       │  RTMW3D           │
│  (cyan)           │  (green)          │
├───────────────────┼───────────────────┤
│  RTMPose + DA V2  │  MediaPipe        │
│  (magenta)        │  (orange)         │
└───────────────────┴───────────────────┘

Usage:
    python3 rom-compare.py [OPTIONS]
    python3 rom-compare.py                           # defaults
    python3 rom-compare.py --input test-data/shoulder-rom/video.mp4

Output: output/<name>_comparison.mp4
"""

import argparse
import os
import sys
import time
from pathlib import Path

import cv2
import numpy as np
import onnxruntime as ort

# ─── COCO keypoint indices (RTMPose/MMPose 17-point) ─────────────────
NOSE = 0
L_EYE, R_EYE = 1, 2
L_EAR, R_EAR = 3, 4
L_SHOULDER, R_SHOULDER = 5, 6
L_ELBOW, R_ELBOW = 7, 8
L_WRIST, R_WRIST = 9, 10
L_HIP, R_HIP = 11, 12
L_KNEE, R_KNEE = 13, 14
L_ANKLE, R_ANKLE = 15, 16

# Subset skeleton for shoulder ROM (10 points)
SKELETON_PAIRS = [
    (L_SHOULDER, R_SHOULDER),   # shoulder line
    (L_SHOULDER, L_ELBOW),      # left upper arm
    (R_SHOULDER, R_ELBOW),      # right upper arm
    (L_ELBOW, L_WRIST),         # left forearm
    (R_ELBOW, R_WRIST),         # right forearm
    (L_SHOULDER, L_HIP),        # left torso
    (R_SHOULDER, R_HIP),        # right torso
    (L_HIP, R_HIP),             # hip line
]

SUBSET_INDICES = sorted(set(
    idx for pair in SKELETON_PAIRS for idx in pair
))

# MediaPipe 33-point → COCO 17-point remapping
MP_TO_COCO = {
    0: NOSE,
    11: L_SHOULDER,
    12: R_SHOULDER,
    13: L_ELBOW,
    14: R_ELBOW,
    15: L_WRIST,
    16: R_WRIST,
    23: L_HIP,
    24: R_HIP,
    25: L_KNEE,
    26: R_KNEE,
    27: L_ANKLE,
    28: R_ANKLE,
}

# Approach display config
APPROACHES = [
    {"name": "RTMPose 2D", "color": (255, 255, 0), "tag": "baseline"},        # cyan
    {"name": "RTMW3D", "color": (0, 255, 0), "tag": "monocular 3D"},          # green
    {"name": "RTMPose+DA", "color": (255, 0, 255), "tag": "pseudo depth"},     # magenta
    {"name": "MediaPipe", "color": (0, 165, 255), "tag": "native 3D"},         # orange
]

# ─── Angle calculation (consistent across all approaches) ─────────────

def vec_angle(a, b, c):
    """Angle at vertex b, between vectors b→a and b→c. Returns degrees."""
    ba = a - b
    bc = c - b
    nba = np.linalg.norm(ba)
    nbc = np.linalg.norm(bc)
    if nba < 1e-6 or nbc < 1e-6:
        return 0.0
    cos_v = np.clip(np.dot(ba, bc) / (nba * nbc), -1.0, 1.0)
    return float(np.degrees(np.arccos(cos_v)))


def angle_from_vertical(shoulder, elbow, hip):
    """Angle between upper-arm vector and torso (hip→shoulder) reference.
    Mirrors JointAngleCalculator::angle_from_vertical in the C++ backend.
    """
    upper_arm = elbow - shoulder
    reference = hip - shoulder
    arm_mag = np.linalg.norm(upper_arm)
    ref_mag = np.linalg.norm(reference)
    if arm_mag < 1e-6 or ref_mag < 1e-6:
        return 0.0
    cos_v = np.clip(np.dot(upper_arm, reference) / (arm_mag * ref_mag), -1.0, 1.0)
    return float(np.degrees(np.arccos(cos_v)))


def compute_angles_2d(kp):
    """Compute shoulder angles from 2D keypoints (N×2 or N×3).
    Returns dict like {'L-Abd': 145.0, 'R-Flex': 172.0}.
    """
    xy = kp[:, :2]
    angles = {}
    # Left side: abduction/flexion = angle_from_vertical(L_SHOULDER, L_ELBOW, L_HIP)
    angles["L-Abd"] = angle_from_vertical(
        xy[L_SHOULDER], xy[L_ELBOW], xy[L_HIP])
    # Right side
    angles["R-Abd"] = angle_from_vertical(
        xy[R_SHOULDER], xy[R_ELBOW], xy[R_HIP])
    return angles


def compute_angles_3d(kp):
    """Compute shoulder angles from 3D keypoints (N×3).
    Uses the same angle_from_vertical but in 3D space.
    """
    angles = {}
    angles["L-Abd"] = angle_from_vertical(
        kp[L_SHOULDER], kp[L_ELBOW], kp[L_HIP])
    angles["R-Abd"] = angle_from_vertical(
        kp[R_SHOULDER], kp[R_ELBOW], kp[R_HIP])
    return angles


# ─── YOLOX person detector ───────────────────────────────────────────

class YOLOXDetector:
    """YOLOX-s ONNX person detector."""

    def __init__(self, model_path: str, input_size=(640, 640), conf_thresh=0.5):
        available = ort.get_available_providers()
        # CoreML misinterprets YOLOX output shape; use CPU
        providers = [p for p in ["CPUExecutionProvider"] if p in available]
        self.session = ort.InferenceSession(model_path, providers=providers)
        self.input_size = input_size
        self.conf_thresh = conf_thresh
        self.input_name = self.session.get_inputs()[0].name

    def detect(self, img_bgr):
        """Returns list of [x1, y1, x2, y2] person boxes."""
        h, w = img_bgr.shape[:2]
        tw, th = self.input_size

        # Resize with padding
        scale = min(tw / w, th / h)
        nw, nh = int(w * scale), int(h * scale)
        resized = cv2.resize(img_bgr, (nw, nh))

        canvas = np.full((th, tw, 3), 114, dtype=np.uint8)
        dx, dy = (tw - nw) // 2, (th - nh) // 2
        canvas[dy:dy + nh, dx:dx + nw] = resized

        blob = canvas.astype(np.float32)  # raw 0-255, no normalization
        blob = blob.transpose(2, 0, 1)[np.newaxis]  # NCHW

        outputs = self.session.run(None, {self.input_name: blob})

        # YOLOX outputs: dets (1, N, 5) + labels (1, N)
        dets = outputs[0]  # (1, N, 5): x1, y1, x2, y2, score
        labels = outputs[1] if len(outputs) > 1 else None  # (1, N): class_id

        if dets.ndim == 3:
            dets = dets[0]
        if labels is not None and labels.ndim == 2:
            labels = labels[0]

        boxes = []
        for i, det in enumerate(dets):
            x1, y1, x2, y2, score = det[:5]

            if score < self.conf_thresh:
                continue

            # Check class (person = 0)
            if labels is not None:
                if int(labels[i]) != 0:
                    continue

            # Map back to original image coords
            bx1 = (x1 - dx) / scale
            by1 = (y1 - dy) / scale
            bx2 = (x2 - dx) / scale
            by2 = (y2 - dy) / scale

            boxes.append([max(0, bx1), max(0, by1), min(w, bx2), min(h, by2)])

        return boxes


# ─── RTMPose keypoint estimator ──────────────────────────────────────

class RTMPoseEstimator:
    """RTMPose-m ONNX keypoint estimator with SimCC output."""

    # ImageNet normalization
    MEAN = np.array([0.485, 0.446, 0.406], dtype=np.float32)
    STD = np.array([0.229, 0.224, 0.225], dtype=np.float32)

    def __init__(self, model_path: str, input_size=(192, 256)):
        available = ort.get_available_providers()
        providers = ["CoreMLExecutionProvider", "CPUExecutionProvider"]
        providers = [p for p in providers if p in available]
        self.session = ort.InferenceSession(model_path, providers=providers)
        self.input_size = input_size  # (w, h)
        self.input_name = self.session.get_inputs()[0].name
        # SimCC sizes: 2× input dimensions
        self.simcc_x_size = input_size[0] * 2  # 384
        self.simcc_y_size = input_size[1] * 2  # 512

    def _preprocess(self, crop_bgr):
        """Preprocess crop: CLAHE + resize + ImageNet normalization."""
        tw, th = self.input_size

        # CLAHE histogram equalization (Lab color space)
        lab = cv2.cvtColor(crop_bgr, cv2.COLOR_BGR2Lab)
        l_ch, a_ch, b_ch = cv2.split(lab)
        clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8))
        l_ch = clahe.apply(l_ch)
        lab = cv2.merge([l_ch, a_ch, b_ch])
        enhanced = cv2.cvtColor(lab, cv2.COLOR_Lab2BGR)

        # Resize to model input
        resized = cv2.resize(enhanced, (tw, th), interpolation=cv2.INTER_LINEAR)

        # BGR→RGB, normalize, HWC→CHW
        rgb = cv2.cvtColor(resized, cv2.COLOR_BGR2RGB)
        normalized = (rgb.astype(np.float32) / 255.0 - self.MEAN) / self.STD
        tensor = normalized.transpose(2, 0, 1)[np.newaxis].astype(np.float32)
        return tensor

    def _refine(self, data, peak_idx, radius=3):
        """Gaussian-weighted sub-pixel refinement."""
        n = len(data)
        sum_v = 0.0
        weight_sum = 0.0
        for i in range(max(0, peak_idx - radius), min(n, peak_idx + radius + 1)):
            dist = float(i - peak_idx)
            weight = np.exp(-dist * dist / 2.0)
            w = weight * data[i]
            sum_v += i * w
            weight_sum += w
        return sum_v / weight_sum if weight_sum > 0 else float(peak_idx)

    def estimate(self, img_bgr, box):
        """Estimate keypoints for a single person box.
        Returns (N, 3) array: x, y, confidence in original image coords.
        """
        h_img, w_img = img_bgr.shape[:2]
        x1, y1, x2, y2 = [int(v) for v in box]

        # Expand bbox by 20% for better context
        bw, bh = x2 - x1, y2 - y1
        ex, ey = int(bw * 0.2), int(bh * 0.2)
        cx1 = max(0, x1 - ex)
        cy1 = max(0, y1 - ey)
        cx2 = min(w_img, x2 + ex)
        cy2 = min(h_img, y2 + ey)
        cw, ch = cx2 - cx1, cy2 - cy1
        if cw <= 0 or ch <= 0:
            return np.zeros((17, 3))

        crop = img_bgr[cy1:cy2, cx1:cx2]

        # Preprocess and run inference
        tensor = self._preprocess(crop)
        outputs = self.session.run(None, {self.input_name: tensor})

        # SimCC outputs: (1, 17, 384) and (1, 17, 512)
        simcc_x = outputs[0][0]  # (17, simcc_x_size)
        simcc_y = outputs[1][0]  # (17, simcc_y_size)

        tw, th = self.input_size
        kps = np.zeros((17, 3))
        for i in range(17):
            sx = simcc_x[i]
            sy = simcc_y[i]

            best_x = int(np.argmax(sx))
            best_y = int(np.argmax(sy))

            # Sub-pixel refinement
            x_refined = self._refine(sx, best_x)
            y_refined = self._refine(sy, best_y)

            # SimCC is 2× input resolution
            x_in = x_refined / 2.0
            y_in = y_refined / 2.0

            # Map from model input space to crop space
            x_crop = x_in * cw / tw
            y_crop = y_in * ch / th

            # Map to original image coords
            kps[i, 0] = x_crop + cx1
            kps[i, 1] = y_crop + cy1
            kps[i, 2] = (sx[best_x] + sy[best_y]) / 2.0

        return kps


# ─── RTMW3D monocular 3D pose estimator ──────────────────────────────

class RTMW3DEstimator:
    """RTMW3D-l ONNX — monocular 3D pose estimation.
    Output: (17, 3) keypoints in camera space (x, y, z).
    Falls back to 2D + zero z if model unavailable.
    """

    def __init__(self, model_path: str = None, input_size=(256, 256)):
        self.available = False
        self.input_size = input_size

        if model_path and os.path.exists(model_path):
            try:
                self.session = ort.InferenceSession(
                    model_path,
                    providers=["CoreMLExecutionProvider", "CPUExecutionProvider"],
                )
                self.input_name = self.session.get_inputs()[0].name
                self.available = True
                print(f"  [RTMW3D] Loaded {model_path}")
            except Exception as e:
                print(f"  [RTMW3D] Failed to load: {e}")
        else:
            print("  [RTMW3D] Model not found — will use 2D+zero-z fallback")

    def estimate_3d(self, img_bgr, box, kps_2d):
        """Estimate 3D keypoints.
        If model available: run RTMW3D.
        Otherwise: return 2D with z=0 (fallback).
        Returns (17, 3) array.
        """
        if not self.available:
            kps_3d = np.zeros((17, 3))
            kps_3d[:, :2] = kps_2d[:, :2]
            return kps_3d

        # Crop and preprocess
        x1, y1, x2, y2 = [int(v) for v in box]
        crop = img_bgr[y1:y2, x1:x2]
        w_box, h_box = x2 - x1, y2 - y1
        tw, th = self.input_size

        resized = cv2.resize(crop, (tw, th))
        blob = resized.astype(np.float32).transpose(2, 0, 1)[np.newaxis] / 255.0

        outputs = self.session.run(None, {self.input_name: blob})

        # RTMW3D output interpretation depends on model version
        # Attempt to reshape to (17, 3)
        raw = outputs[0]
        if raw.ndim >= 2:
            raw = raw.reshape(-1, raw.shape[-1])
        if raw.shape[0] >= 17 and raw.shape[1] >= 3:
            kps_3d = raw[:17, :3].copy()
        elif raw.shape[0] >= 17:
            kps_3d = np.zeros((17, 3))
            kps_3d[:, :raw.shape[1]] = raw[:17, :]
        else:
            kps_3d = np.zeros((17, 3))
            kps_3d[:, :2] = kps_2d[:, :2]

        return kps_3d


# ─── Depth Anything V2 estimator ─────────────────────────────────────

class DepthAnythingEstimator:
    """Depth Anything V2 ViT-S via ONNX Runtime.
    Generates a relative depth map for pseudo-3D lifting.
    """

    def __init__(self, model_path: str = None):
        self.available = False

        if model_path and os.path.exists(model_path):
            try:
                self.session = ort.InferenceSession(
                    model_path,
                    providers=["CoreMLExecutionProvider", "CPUExecutionProvider"],
                )
                self.input_name = self.session.get_inputs()[0].name
                self.input_shape = self.session.get_inputs()[0].shape
                self.available = True
                print(f"  [DepthAnything] Loaded {model_path}")
            except Exception as e:
                print(f"  [DepthAnything] Failed to load: {e}")
        else:
            print("  [DepthAnything] Model not found — pseudo-depth lifting disabled")

    def estimate_depth(self, img_bgr):
        """Returns relative depth map (H, W), float32, same spatial dims as input."""
        if not self.available:
            return None

        h, w = img_bgr.shape[:2]
        # Typical DA V2 input: 518×518 or model-specified
        inp_h, inp_w = 518, 518
        if isinstance(self.input_shape, list) and len(self.input_shape) == 4:
            try:
                inp_h = int(self.input_shape[2]) or 518
                inp_w = int(self.input_shape[3]) or 518
            except (TypeError, ValueError):
                pass

        resized = cv2.resize(img_bgr, (inp_w, inp_h))
        blob = resized.astype(np.float32).transpose(2, 0, 1)[np.newaxis] / 255.0

        outputs = self.session.run(None, {self.input_name: blob})
        depth = outputs[0]

        if depth.ndim == 4:
            depth = depth[0, 0]
        elif depth.ndim == 3:
            depth = depth[0]

        # Resize back to original dims
        depth = cv2.resize(depth.astype(np.float32), (w, h))
        return depth

    def lift_2d_to_3d(self, kps_2d, depth_map, focal_length=None):
        """Lift 2D keypoints to 3D using depth map.
        Returns (17, 3) array with x, y in pixels, z = depth at keypoint.
        """
        if depth_map is None:
            kps_3d = np.zeros((17, 3))
            kps_3d[:, :2] = kps_2d[:, :2]
            return kps_3d

        h, w = depth_map.shape[:2]
        kps_3d = np.zeros((17, 3))
        kps_3d[:, :2] = kps_2d[:, :2]

        for i in range(17):
            x, y = int(np.clip(kps_2d[i, 0], 0, w - 1)), int(np.clip(kps_2d[i, 1], 0, h - 1))
            # Sample depth in a small window for robustness
            r = 3
            y1_s = max(0, y - r)
            y2_s = min(h, y + r + 1)
            x1_s = max(0, x - r)
            x2_s = min(w, x + r + 1)
            region = depth_map[y1_s:y2_s, x1_s:x2_s]
            if region.size > 0:
                kps_3d[i, 2] = float(np.median(region))
            else:
                kps_3d[i, 2] = 0.0

        return kps_3d


# ─── MediaPipe Pose ──────────────────────────────────────────────────

class MediaPipeEstimator:
    """MediaPipe Pose with 33 landmarks → remapped to COCO 17-point."""

    def __init__(self):
        try:
            from mediapipe.tasks.python import vision as mp_vision
            from mediapipe.tasks.python.core import base_options as mp_base
            import mediapipe as mp

            # Download model if needed
            model_path = self._get_model_path()

            base_opts = mp_base.BaseOptions(model_asset_path=model_path)
            opts = mp_vision.PoseLandmarkerOptions(
                base_options=base_opts,
                running_mode=mp_vision.RunningMode.VIDEO,
                min_pose_detection_confidence=0.5,
                min_pose_presence_confidence=0.5,
                min_tracking_confidence=0.5,
                num_poses=1,
            )
            self.landmarker = mp_vision.PoseLandmarker.create_from_options(opts)
            self.mp_vision = mp_vision
            self.mp = mp
            self.available = True
            print("  [MediaPipe] Loaded (PoseLandmarker)")
        except Exception as e:
            self.available = False
            print(f"  [MediaPipe] Failed to load: {e}")

    def _get_model_path(self):
        """Get or download the pose landmarker model."""
        import urllib.request
        model_dir = Path("models")
        model_dir.mkdir(exist_ok=True)
        model_path = model_dir / "pose_landmarker_heavy.task"
        if not model_path.exists():
            url = "https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_heavy/float16/latest/pose_landmarker_heavy.task"
            print(f"  [MediaPipe] Downloading model...")
            urllib.request.urlretrieve(url, str(model_path))
            print(f"  [MediaPipe] Model saved to {model_path}")
        return str(model_path)

    def estimate(self, img_bgr, timestamp_ms=0):
        """Returns (17, 3) COCO-format keypoints with x, y, confidence.
        Also returns 33-landmark raw data with z for 3D angles.
        """
        if not self.available:
            return np.zeros((17, 3)), None

        h, w = img_bgr.shape[:2]
        rgb = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2RGB)
        mp_image = self.mp.Image(image_format=self.mp.ImageFormat.SRGB, data=rgb)

        result = self.landmarker.detect_for_video(mp_image, timestamp_ms)

        if not result.pose_landmarks:
            return np.zeros((17, 3)), None

        landmarks = result.pose_landmarks[0]  # first person

        # Build full 33-landmark array (x_px, y_px, z, visibility)
        mp_kps = np.zeros((33, 4))
        for i, lm in enumerate(landmarks):
            mp_kps[i] = [lm.x * w, lm.y * h, lm.z, lm.visibility]

        # Remap to COCO 17-point
        coco_kps = np.zeros((17, 3))
        for mp_idx, coco_idx in MP_TO_COCO.items():
            coco_kps[coco_idx] = mp_kps[mp_idx, :3]  # x, y, z
            coco_kps[coco_idx, 2] = mp_kps[mp_idx, 3]  # use visibility as confidence

        return coco_kps, mp_kps

    def compute_angles_3d(self, mp_kps):
        """Compute 3D angles directly from MediaPipe's 33 landmarks.
        Uses z coordinate for true 3D angle calculation.
        """
        if mp_kps is None:
            return {"L-Abd": 0.0, "R-Abd": 0.0}

        angles = {}
        # Left: 11=shoulder, 13=elbow, 23=hip
        l_shoulder = mp_kps[11, :3]
        l_elbow = mp_kps[13, :3]
        l_hip = mp_kps[23, :3]
        angles["L-Abd"] = angle_from_vertical(l_shoulder, l_elbow, l_hip)

        # Right: 12=shoulder, 14=elbow, 24=hip
        r_shoulder = mp_kps[12, :3]
        r_elbow = mp_kps[14, :3]
        r_hip = mp_kps[24, :3]
        angles["R-Abd"] = angle_from_vertical(r_shoulder, r_elbow, r_hip)

        return angles


# ─── Visualization ────────────────────────────────────────────────────

def draw_skeleton(img, kps, color, thickness=2, radius=4):
    """Draw subset skeleton (shoulders, elbows, wrists, torso) on image."""
    h, w = img.shape[:2]

    for i, j in SKELETON_PAIRS:
        if i >= len(kps) or j >= len(kps):
            continue
        p1 = (int(kps[i, 0]), int(kps[i, 1]))
        p2 = (int(kps[j, 0]), int(kps[j, 1]))
        if 0 <= p1[0] < w and 0 <= p1[1] < h and 0 <= p2[0] < w and 0 <= p2[1] < h:
            cv2.line(img, p1, p2, color, thickness, cv2.LINE_AA)

    for idx in SUBSET_INDICES:
        if idx >= len(kps):
            continue
        p = (int(kps[idx, 0]), int(kps[idx, 1]))
        if 0 <= p[0] < w and 0 <= p[1] < h:
            cv2.circle(img, p, radius, color, -1, cv2.LINE_AA)


def draw_angles(img, angles, color, origin=(10, 30)):
    """Draw angle text overlay."""
    x, y = origin
    for label, val in angles.items():
        text = f"{label}: {val:.0f}deg"
        cv2.putText(img, text, (x, y),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 0), 3, cv2.LINE_AA)
        cv2.putText(img, text, (x, y),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, color, 1, cv2.LINE_AA)
        y += 28


def draw_label(img, name, tag, color, origin=(10, None)):
    """Draw approach label at bottom-left of quadrant."""
    x, y = origin
    if y is None:
        y = img.shape[0] - 15
    text = f"{name} ({tag})"
    cv2.putText(img, text, (x, y),
                cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 0, 0), 3, cv2.LINE_AA)
    cv2.putText(img, text, (x, y),
                cv2.FONT_HERSHEY_SIMPLEX, 0.55, color, 1, cv2.LINE_AA)


def make_2x2_grid(quads, frame_w, frame_h):
    """Arrange 4 quadrant images into a 2×2 grid.
    Each quad is resized to (frame_w//2, frame_h//2).
    """
    hw, hh = frame_w // 2, frame_h // 2
    grid = np.zeros((frame_h, frame_w, 3), dtype=np.uint8)

    positions = [(0, 0), (0, hw), (hh, 0), (hh, hw)]
    for i, (qy, qx) in enumerate(positions):
        if i < len(quads):
            resized = cv2.resize(quads[i], (hw, hh))
            grid[qy:qy + hh, qx:qx + hw] = resized

    # Draw grid lines
    cv2.line(grid, (hw, 0), (hw, frame_h), (128, 128, 128), 1)
    cv2.line(grid, (0, hh), (frame_w, hh), (128, 128, 128), 1)

    return grid


# ─── Main pipeline ────────────────────────────────────────────────────

def run_comparison(args):
    """Main comparison pipeline."""
    input_path = args.input
    if not os.path.exists(input_path):
        print(f"Error: Input file not found: {input_path}")
        sys.exit(1)

    # Prepare output path
    input_stem = Path(input_path).stem
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    output_path = output_dir / f"{input_stem}_comparison.mp4"

    print(f"Input:  {input_path}")
    print(f"Output: {output_path}")

    # Open video
    cap = cv2.VideoCapture(input_path)
    if not cap.isOpened():
        print(f"Error: Cannot open video: {input_path}")
        sys.exit(1)

    fps = cap.get(cv2.CAP_PROP_FPS) or 30.0
    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    frame_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    frame_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    print(f"Video: {frame_w}x{frame_h} @ {fps:.1f}fps, {total_frames} frames")

    # Limit output resolution
    out_w = min(args.max_width, frame_w)
    out_h = min(args.max_height, frame_h)
    # Ensure even dimensions
    out_w = out_w - (out_w % 2)
    out_h = out_h - (out_h % 2)

    # Initialize pipelines
    print("\n=== Initializing pipelines ===")

    print("[1/4] RTMPose 2D (YOLOX + RTMPose-m)")
    detector = YOLOXDetector(args.yolox_model)
    rtmpose = RTMPoseEstimator(args.rtmpose_model)

    print("[2/4] RTMW3D")
    rtmw3d = RTMW3DEstimator(args.rtmw3d_model)

    print("[3/4] RTMPose + Depth Anything V2")
    depth_anything = DepthAnythingEstimator(args.da_model)

    print("[4/4] MediaPipe Pose")
    mediapipe_est = MediaPipeEstimator()

    # Video writer
    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
    writer = cv2.VideoWriter(str(output_path), fourcc, fps, (out_w, out_h))

    if not writer.isOpened():
        print(f"Error: Cannot open video writer for {output_path}")
        sys.exit(1)

    print(f"\n=== Processing {total_frames} frames ===")

    frame_idx = 0
    t_start = time.time()
    da_time_acc = 0.0  # track DA time separately
    depth_map_cache = None

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        frame_idx += 1

        # Skip frames if requested
        if args.skip > 0 and frame_idx % (args.skip + 1) != 0:
            continue

        quads = [None] * 4
        all_angles = [{}] * 4

        # ─── Pipeline 1: RTMPose 2D ──────────────────────────────────
        try:
            boxes = detector.detect(frame)
            if boxes:
                # Use largest box (closest person)
                box = max(boxes, key=lambda b: (b[2] - b[0]) * (b[3] - b[1]))
                kps_2d = rtmpose.estimate(frame, box)
                angles = compute_angles_2d(kps_2d)
            else:
                kps_2d = np.zeros((17, 3))
                angles = {"L-Abd": 0.0, "R-Abd": 0.0}

            q1 = frame.copy()
            draw_skeleton(q1, kps_2d, APPROACHES[0]["color"])
            draw_angles(q1, angles, APPROACHES[0]["color"])
            draw_label(q1, APPROACHES[0]["name"], APPROACHES[0]["tag"], APPROACHES[0]["color"])
            quads[0] = q1
            all_angles[0] = angles
        except Exception as e:
            quads[0] = frame.copy()
            if args.verbose:
                print(f"  Frame {frame_idx} Pipeline 1 error: {e}")

        # ─── Pipeline 2: RTMW3D ──────────────────────────────────────
        try:
            # Reuse box from pipeline 1
            if boxes:
                kps_3d = rtmw3d.estimate_3d(frame, box, kps_2d)
                angles = compute_angles_3d(kps_3d)
            else:
                kps_3d = np.zeros((17, 3))
                angles = {"L-Abd": 0.0, "R-Abd": 0.0}

            q2 = frame.copy()
            draw_skeleton(q2, kps_3d, APPROACHES[1]["color"])
            draw_angles(q2, angles, APPROACHES[1]["color"])
            draw_label(q2, APPROACHES[1]["name"], APPROACHES[1]["tag"], APPROACHES[1]["color"])
            quads[1] = q2
            all_angles[1] = angles
        except Exception as e:
            quads[1] = frame.copy()
            if args.verbose:
                print(f"  Frame {frame_idx} Pipeline 2 error: {e}")

        # ─── Pipeline 3: RTMPose + Depth Anything ────────────────────
        try:
            if boxes and depth_anything.available:
                t_da = time.time()
                # Cache depth map (expensive, reuse across frames if skip>0)
                if depth_map_cache is None or args.skip == 0:
                    depth_map_cache = depth_anything.estimate_depth(frame)
                da_time_acc += time.time() - t_da

                kps_3d_da = depth_anything.lift_2d_to_3d(kps_2d, depth_map_cache)
                angles = compute_angles_3d(kps_3d_da)
            elif boxes:
                # No depth model — show 2D with note
                kps_3d_da = np.zeros((17, 3))
                kps_3d_da[:, :2] = kps_2d[:, :2]
                angles = compute_angles_2d(kps_2d)
            else:
                kps_3d_da = np.zeros((17, 3))
                angles = {"L-Abd": 0.0, "R-Abd": 0.0}

            q3 = frame.copy()
            draw_skeleton(q3, kps_3d_da, APPROACHES[2]["color"])
            draw_angles(q3, angles, APPROACHES[2]["color"])
            draw_label(q3, APPROACHES[2]["name"], APPROACHES[2]["tag"], APPROACHES[2]["color"])
            if not depth_anything.available:
                cv2.putText(q3, "(no depth model)", (10, 60),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (128, 128, 128), 1, cv2.LINE_AA)
            quads[2] = q3
            all_angles[2] = angles
        except Exception as e:
            quads[2] = frame.copy()
            if args.verbose:
                print(f"  Frame {frame_idx} Pipeline 3 error: {e}")

        # ─── Pipeline 4: MediaPipe ───────────────────────────────────
        try:
            coco_kps, mp_kps_raw = mediapipe_est.estimate(frame, int(frame_idx * 1000 / fps))

            # 2D angles from remapped COCO keypoints
            angles_2d = compute_angles_2d(coco_kps)
            # 3D angles from native MediaPipe z-coordinates
            angles_3d = mediapipe_est.compute_angles_3d(mp_kps_raw)
            # Use 3D angles if available, fall back to 2D
            angles = angles_3d if angles_3d else angles_2d

            q4 = frame.copy()
            draw_skeleton(q4, coco_kps, APPROACHES[3]["color"])
            draw_angles(q4, angles, APPROACHES[3]["color"])
            draw_label(q4, APPROACHES[3]["name"], APPROACHES[3]["tag"], APPROACHES[3]["color"])
            quads[3] = q4
            all_angles[3] = angles
        except Exception as e:
            quads[3] = frame.copy()
            if args.verbose:
                print(f"  Frame {frame_idx} Pipeline 4 error: {e}")

        # ─── Compose 2×2 grid ────────────────────────────────────────
        grid = make_2x2_grid(quads, out_w, out_h)
        writer.write(grid)

        # Progress
        if frame_idx % 30 == 0 or frame_idx == total_frames:
            elapsed = time.time() - t_start
            rate = frame_idx / elapsed if elapsed > 0 else 0
            eta = (total_frames - frame_idx) / rate if rate > 0 else 0
            print(f"  [{frame_idx}/{total_frames}] "
                  f"{rate:.1f} fps, ETA {eta:.0f}s"
                  f" | L-Abd: RTMPose={all_angles[0].get('L-Abd', 0):.0f} "
                  f"RTMW3D={all_angles[1].get('L-Abd', 0):.0f} "
                  f"DA={all_angles[2].get('L-Abd', 0):.0f} "
                  f"MP={all_angles[3].get('L-Abd', 0):.0f}")

    cap.release()
    writer.release()

    elapsed = time.time() - t_start
    print(f"\n=== Done ===")
    print(f"Processed {frame_idx} frames in {elapsed:.1f}s ({frame_idx / elapsed:.1f} fps)")
    if depth_anything.available and da_time_acc > 0:
        print(f"Depth Anything: {da_time_acc:.1f}s total ({da_time_acc / frame_idx * 1000:.0f}ms/frame avg)")
    print(f"Output: {output_path}")


# ─── CLI ──────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="2×2 comparison of shoulder ROM estimation approaches",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Approaches:
  1. RTMPose 2D    — YOLOX detection + RTMPose-m keypoints → 2D angles
  2. RTMW3D        — Monocular 3D pose estimation → 3D angles
  3. RTMPose + DA  — RTMPose 2D + Depth Anything V2 pseudo-depth → 3D angles
  4. MediaPipe     — Native 33-landmark pose with z-coordinates → 3D angles

Examples:
  python3 rom-compare.py
  python3 rom-compare.py --input video.mp4 --skip 2
  python3 rom-compare.py --da-model models/depth_anything_v2_vits.onnx
        """,
    )

    parser.add_argument("--input", "-i", default=None,
                        help="Input MP4 path (auto-detected from test-data/ if omitted)")
    parser.add_argument("--output-dir", "-o", default="output",
                        help="Output directory (default: output)")
    parser.add_argument("--skip", type=int, default=0,
                        help="Process every N-th frame (0 = every frame)")
    parser.add_argument("--max-width", type=int, default=1920,
                        help="Max output width (default: 1920)")
    parser.add_argument("--max-height", type=int, default=1080,
                        help="Max output height (default: 1080)")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Print per-frame error details")

    # Model paths
    parser.add_argument("--yolox-model", default="models/yolox-s.onnx",
                        help="YOLOX-s ONNX model path")
    parser.add_argument("--rtmpose-model", default="models/rtmpose-m.onnx",
                        help="RTMPose-m ONNX model path")
    parser.add_argument("--rtmw3d-model", default=None,
                        help="RTMW3D-l ONNX model path (optional)")
    parser.add_argument("--da-model", default=None,
                        help="Depth Anything V2 ViT-S ONNX model path (optional)")

    args = parser.parse_args()

    # Auto-detect input
    if args.input is None:
        test_dir = Path("test-data")
        mp4s = list(test_dir.rglob("*.mp4"))
        if mp4s:
            args.input = str(mp4s[0])
            print(f"Auto-detected input: {args.input}")
        else:
            print("Error: No --input specified and no .mp4 found in test-data/")
            sys.exit(1)

    run_comparison(args)


if __name__ == "__main__":
    main()