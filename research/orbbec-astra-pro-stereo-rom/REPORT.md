# Improving Shoulder ROM Accuracy with Orbbec Astra Pro Stereo/Depth Camera and Point Cloud Processing

> Generated 2026-10-09 · depth: standard · 87 sources · workspace: research/orbbec-astra-pro-stereo-rom/

## Executive summary

- The Orbbec Astra Pro Plus is a structured-light depth camera (850nm IR) with 640x480 depth at 30fps, ±3mm precision at 1m, and 0.6–8m range — suitable for clinical shoulder ROM distances (0.5–2m) [1][2].
- The Astra Pro Plus is in "limited maintenance" mode in Orbbec SDK v1 and "not supported" in SDK v2; it only works via the legacy Astra SDK 2.1.3 (EOL 2022Q3) or OpenNI [3][4]. Upgrading to an Astra 2 would provide better long-term SDK support.
- The canonical hybrid pipeline — "detect 2D from RGB, lift to 3D with depth" — consistently outperforms both color-only and depth-only approaches [5][6]. Zimmermann et al. (2018) showed this cuts 3D keypoint error in half (11.2 vs 22.7 cm) [5].
- SimpleDepthPose (Jan 2025) provides the most directly applicable lifting approach: look up depth at 2D keypixel locations using a cross-shaped median filter with per-joint offsets (+3cm for shoulders), requiring no training and running in ~3ms [7].
- RGB-D cameras achieve clinically acceptable LOA (±10°) for simple shoulder movements (flexion, abduction) but struggle with rotational movements (internal rotation LOA can widen to -21° to +8°) [8][9].
- Point cloud processing for shoulder geometry is well-supported by PCL and Open3D; the recommended pipeline is: RTMPose 2D keypoints → depth map ROI crop → point cloud extraction → normal estimation → RANSAC plane fitting [10][11].
- RTMPose inference on Jetson AGX Orin with TensorRT FP16 runs at 2.72ms (medium), well within real-time budgets [12].
- No published study directly benchmarks the Orbbec Astra Pro for shoulder ROM accuracy; all evidence comes from Kinect V2/Azure Kinect/RealSense studies [9].

## Background & scope

This report addresses how to improve shoulder ROM measurement accuracy using an Orbbec Astra Pro stereo/depth camera and point cloud processing. The current pipeline uses RTMPose 2D keypoints from monocular video. Scope includes hardware capabilities, calibration, depth-enhanced pose, 2D-to-3D lifting, point cloud processing, hybrid pipeline design, accuracy benchmarks, and implementation considerations. Hardware is already selected (Orbbec Astra Pro); camera recommendations are out of scope.

## 1. Hardware & SDK capabilities

The Astra Pro Plus uses structured light depth technology with an 850nm infrared projector and Orbbec-designed ASIC for on-device depth processing [1]. Key specs:

- Depth range: 0.6–8m (0.4–2m for Mini S variant)
- Precision: ±3mm at 1m
- Depth resolution: up to 640x480 at 30fps
- RGB resolution: up to 1920x1080 at 30fps (Pro Plus)
- Depth FOV: H58.4° × V45.5°
- Connection: USB 2.0 Type-A, power consumption <2.4W
- Dimensions: 165mm × 48mm × 40mm, 310g [1][2]

SDK support is a significant constraint:

- Orbbec SDK v1 (v1.10.37): "limited maintenance" for Astra Pro Plus (critical bug fixes only)
- Orbbec SDK v2: "not supported" for Astra Pro Plus
- Astra SDK 2.1.3: End-of-Life since 2022Q3
- Orbbec OpenNI SDK (v2.3.0.86): supports Astra Pro Plus on Linux (Ubuntu 14.04+), Windows, Android [3][4]

The Orbbec SDK v1 supports Linux ARM64 on Ubuntu 18.04/20.04/22.04 with GCC 7.5, including NVIDIA Jetson AGX Orin, Orin NX, Orin Nano, AGX Xavier, Xavier NX, Jetson Nano, A311D, Raspberry Pi 4, and RK3399 [4].

**Recommendation:** Consider upgrading to Orbbec Astra 2 for full SDK v2 support, built-in undistortion filters, and active development. The Astra Pro Plus is a legacy device that will not receive new features.

## 2. Calibration techniques

ChArUco boards are strictly recommended over plain checkerboards for calibration — OpenCV docs state ChArUco corners are "much more accurate" and tolerate partial views/occlusions [13]. The standard calibration pipeline uses:

1. `cv::calibrateCamera` with Zhang's method to estimate intrinsic matrix K and distortion coefficients [14]
2. `cv::stereoCalibrate` for depth-to-RGB extrinsic alignment [15]
3. `cv::initUndistortRectifyMap` + `cv::remap` for real-time distortion correction (precompute remap tables once) [14]

Factory calibration is stored in device firmware and accessible via SDK APIs; user-side recalibration may be needed for improved accuracy at ROM measurement distances [3].

The pyorbbecsdk Python SDK (v2) provides built-in `AlignFilter`, `PointCloudFilter`, and calibration data access, but does NOT support the original Astra Pro (only newer devices) [16].

## 3. Depth-enhanced 2D pose

Zimmermann et al. (ICRA 2018) demonstrated that fusing OpenPose 2D keypoints with depth via VoxelPoseNet cuts 3D keypoint error in half (11.2 vs 22.7 cm), with color disambiguating left/right and depth enabling occluded keypoint inference [5]. A naive "pick depth at 2D keypoint" baseline is feasible but fragile under occlusion and depth noise [5].

Key findings for shoulder ROM specifically:

- Depth enables measurement of out-of-plane angles that 2D systems miss entirely [17]
- Pure depth-only trackers (like Kinect SDK) fail at left/right disambiguation when the person faces away — color+depth fusion is essential [5]
- Lifting occluded poses in 3D space outperforms 2D-space completion by 7.9% [18]
- LiDAR depth on iPad enables accurate shoulder abduction angle measurement beyond what 2D pose systems can achieve [17]

## 4. 2D-to-3D pose lifting

SimpleDepthPose (Jan 2025) is the most directly applicable approach: it performs 2D-to-3D lifting by looking up depth at 2D keypoint pixel locations using a cross-shaped median filter with per-joint offsets (+3cm for shoulders). It requires no training, runs in ~3ms, and achieves best-in-class multi-view accuracy (PCP 74.0%, MPJPE 113mm on MVOR) [7].

AugLift (Aug 2025, updated Apr 2026) enriches any 2D-to-3D lifter with a 6D depth-aware descriptor (UADD) from monocular depth maps, reducing cross-dataset MPJPE by 10.1%. It is composable with any lifting architecture (PoseFormer, MotionBERT, MotionAGFormer) by simply widening the input layer from 2K to 6K channels [19].

Oracle experiments show that even coarse ordinal depth reduces lifting error by ~25%, confirming that a real depth sensor like the Orbbec Astra Pro should provide substantial gains over monocular approaches [19].

## 5. Point cloud shoulder geometry

PCL and Open3D provide well-supported libraries for point cloud processing:

- Normal estimation: PCA on k-nearest-neighbor covariance matrix; `NormalEstimationOMP` for multi-threaded; `NormalEstimationUsingIntegralImages` for organized depth data [10]
- Plane segmentation: `SACSegmentation` with `SACMODEL_PLANE` and `SAC_RANSAC` [11]
- Iterative multi-plane extraction: VoxelGrid downsampling + repeated RANSAC + ExtractIndices [11]
- Open3D: `segment_plane()` and `estimate_normals()` in Python with no C++ compilation [20]

For shoulder geometry specifically:

- Abromavicius et al. (2026) demonstrated calibration-free shoulder kinematics from RGB-D point clouds using signed distance fields [21]
- Rozevink et al. (2018) showed robust shoulder rotation center estimation via repeated RANSAC on 3D body scan point clouds [22]
- Normal orientation consistency can be achieved by flipping normals toward the known viewpoint [10]

**Recommended pipeline:** RTMPose 2D keypoints → depth map ROI crop → point cloud extraction → normal estimation (radius 0.01–0.05m) → RANSAC plane fitting for glenoid + cylinder fitting for humerus.

## 6. Hybrid pipeline design

The canonical hybrid pipeline architecture is "detect 2D from RGB, lift to 3D with depth" (Zimmermann 2018, ICRA, 257 citations), which consistently outperforms color-only or depth-only approaches [5][6]. RTMW extends RTMPose for 3D via coordinate classification but uses monocular depth priors only — adding sensor depth would improve it [23].

Azure Kinect studies confirm that depth sensor quality directly impacts shoulder ROM accuracy [24]. A validated clinical shoulder ROM system (Gauci 2023) already uses exactly this RGB-D point cloud pipeline [25]. Monocular HPE for shoulder ROM achieves <10° RMSE in 8/10 movements but perspective distortion is a key failure mode that depth resolves [26].

The primary failure mode in depth-based pose is joint occlusion in the point cloud [27].

## 7. Accuracy benchmarks

A 2024 meta-analysis (15 studies, 608 shoulders) found digital shoulder ROM devices have a pooled mean bias of only -0.25° vs goniometry, with no significant difference between RGB-D/depth cameras and 3D motion analysis systems (p=0.83) [8].

Kinect V2 RGB-D achieves LOA within ±10° (clinically acceptable) for simple movements like flexion and abduction, but internal rotation LOA can widen to -21° to +8° [8][9]. A 2025 systematic review of 14 studies (12 using Kinect V2, 3 using Azure Kinect) confirms good-to-excellent validity for simple movements but inconsistent results for rotational/complex movements [9].

An RGB-D camera (Intel L515) for lower-limb ROM achieved maximum error of 2.2° [28]. Orbbec Astra Pro is named alongside Kinect V2/Azure Kinect/RealSense as a candidate RGB-D sensor, but no study has directly benchmarked it for ROM accuracy yet [9].

## 8. Implementation & latency

RTMPose inference on Jetson AGX Orin with TensorRT FP16 runs at 1.63ms (tiny) to 3.67ms (large) at 256x192, and Orin NX is ~15-30% slower — well within real-time budgets [12]. Orbbec SDK v2 officially supports Jetson AGX Orin, Orin NX, Orin Nano, Xavier, and Thor on ARM64 [29].

Switching from LibUVC to V4L2 backend and increasing usbfs buffer to 128MB are recommended tuning steps for low-latency depth streaming [30]. Orbbec's OpenNI SDK is in "limited maintenance" with active migration to UVC protocol — Orbbec SDK v2 is the only recommended path for current devices [29].

No direct OpenNI2-vs-Orbbec-SDK benchmark or RK3588-specific latency data was found in primary sources.

## Comparison table

| Approach | Accuracy (LOA) | Latency | Complexity | Sources |
|----------|----------------|---------|------------|---------|
| Monocular RTMPose (current) | <10° RMSE (8/10 movements) | ~3ms | Low | [26] |
| Kinect V2 RGB-D skeleton | ±10° (simple), ±21° (rotation) | ~30ms | Medium | [8][9] |
| Azure Kinect RGB-D skeleton | ±10° (simple), better rotation | ~30ms | Medium | [9] |
| SimpleDepthPose (depth lookup) | MPJPE 113mm (MVOR) | ~3ms | Low | [7] |
| AugLift (depth-enriched lifting) | -10.1% MPJPE | ~3ms + lifter | Medium | [19] |
| Point cloud RANSAC (glenoid) | Not benchmarked for ROM | ~10-50ms | High | [10][11][21] |

## Open questions

1. What is the actual achieved point cloud density and noise floor of the Astra Pro at 0.6–1.5m (shoulder ROM distance)? No public data found.
2. Does the Astra Pro's factory calibration provide sufficient accuracy for joint angle measurement, or is user-side ChArUco calibration needed?
3. Would upgrading to Orbbec Astra 2 (full SDK v2 support, built-in undistortion) provide measurably better ROM accuracy?
4. Can SimpleDepthPose's per-joint depth offsets (3cm for shoulders) be calibrated against ground-truth motion capture for <5° accuracy?
5. What is the end-to-end pipeline latency on target hardware (Jetson Orin NX or RK3588) for depth frame acquisition + alignment + RTMPose inference + depth lookup?
6. Are there published studies specifically combining RTMPose 2D keypoints with structured-light depth cameras (not just Azure Kinect) for joint angle measurement?

## Sources

[1] Orbbec Astra Series — https://www.orbbec.com/products/structured-light-camera/astra-series/ (accessed 2026-10-09)
[2] Orbbec OpenNI SDK — https://www.orbbec.com/developers/openni-sdk/ (accessed 2026-10-09)
[3] Orbbec Astra SDK EOL — https://www.orbbec.com/developers/astra-sdk/ (accessed 2026-10-09)
[4] OrbbecSDK GitHub — https://github.com/orbbec/OrbbecSDK (accessed 2026-10-09)
[5] Zimmermann et al. (ICRA 2018) "3D Human Pose Estimation in RGBD" — https://arxiv.org/abs/1803.02622 (published 2018-03-07)
[6] Ying & Zhao (ICIP 2021) "RGB-D Fusion for 3D Human Pose" — https://ieeexplore.ieee.org/abstract/document/9506588/ (published 2021)
[7] SimpleDepthPose (2025) — https://arxiv.org/abs/2501.18478 (published 2025-01-30)
[8] Shepherd et al. (2023) "Meta-analysis of digital shoulder ROM devices" — https://pmc.ncbi.nlm.nih.gov/articles/PMC11418675/ (published 2023-08-31)
[9] Frontiers (2025) "Systematic review of RGB-D shoulder ROM" — https://www.frontiersin.org/journals/bioengineering-and-biotechnology/articles/10.3389/fbioe.2025.1570637/full (published 2025-05-23)
[10] PCL Normal Estimation Tutorial — https://pcl.readthedocs.io/projects/tutorials/en/latest/normal_estimation.html (accessed 2026-10-09)
[11] PCL Planar Segmentation Tutorial — https://pcl.readthedocs.io/projects/tutorials/en/latest/planar_segmentation.html (accessed 2026-10-09)
[12] MMPose RTMPose Benchmark — https://github.com/open-mmlab/mmpose/blob/main/projects/rtmpose/benchmark/README.md (accessed 2026-10-09)
[13] OpenCV ChArUco Calibration — https://docs.opencv.org/4.13.0/da/d13/tutorial_aruco_calibration.html (published 2025-12-31)
[14] LearnOpenCV Camera Calibration — https://learnopencv.com/camera-calibration-using-opencv/ (updated 2026-07-31)
[15] OpenCV calib3d Documentation — https://docs.opencv.org/4.13.0/d9/d0c/group__calib3d.html (published 2025-12-31)
[16] pyorbbecsdk GitHub — https://github.com/orbbec/pyorbbecsdk (accessed 2026-10-09)
[17] Khanghah et al. (2024) "LiDAR depth for shoulder ROM" — https://link.springer.com/article/10.1186/s12938-024-01203-5 (published 2024)
[18] Hardy & Kim (WACV 2024) "Lifting Independent Keypoints" — https://arxiv.org/abs/2309.07243 (published 2023-09-13)
[19] AugLift (2025/2026) — https://arxiv.org/abs/2508.07112 (published 2025-08-09, updated 2026-04-07)
[20] Open3D PointCloud API — https://www.open3d.org/docs/release/python_api/open3d.geometry.PointCloud.html (accessed 2026-10-09)
[21] Abromavicius et al. (IEEE Sensors 2026) — https://ieeexplore.ieee.org/abstract/document/11368759/ (published 2026)
[22] Rozevink et al. (3DBODY.TECH 2018) — https://www.academia.edu/download/77108226/18019rozevink.pdf (published 2018)
[23] RTMW (2024) — https://arxiv.org/abs/2407.08634 (published 2024-07-11)
[24] Ozsoy et al. (J Shoulder Elbow Surg 2022) — https://www.sciencedirect.com/science/article/pii/S1058274622004347 (published 2022)
[25] Gauci et al. (Int Orthop 2023) — https://link.springer.com/article/10.1007/s00264-022-05675-9 (published 2023)
[26] Moreira et al. (2024) "Monocular HPE for shoulder ROM" — https://pmc.ncbi.nlm.nih.gov/articles/PMC11679233/ (published 2024-12-14)
[27] Sarsfield et al. (Int J Med Inform 2019) — https://www.sciencedirect.com/science/article/pii/S1386505618312759 (published 2019)
[28] Frontiers Neurorobotics (2021) "RGB-D lower limb ROM" — https://www.frontiersin.org/journals/neurorobotics/articles/10.3389/fnbot.2021.753924/full (published 2021-10-15)
[29] OrbbecSDK_v2 GitHub — https://github.com/orbbec/OrbbecSDK_v2 (accessed 2026-10-09)
[30] Orbbec Performance Tuning — https://github.com/orbbec/OrbbecSDK_v2/blob/main/docs/tutorial/performance_tuning.md (accessed 2026-10-09)