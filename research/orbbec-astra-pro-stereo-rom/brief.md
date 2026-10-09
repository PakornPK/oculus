# Research Brief

**Date:** 2026-10-09
**Mode:** standard

## Question

How can we improve shoulder ROM measurement accuracy using an Orbbec Astra Pro stereo/depth camera and point cloud processing?

## Scope

### In
- Orbbec Astra Pro hardware capabilities (RGB + depth sensors, SDK, resolution, accuracy specs)
- Stereo/depth camera calibration techniques for joint angle measurement
- 2D-to-3D pose lifting approaches using depth data
- Point cloud processing for body/shoulder geometry
- Integration with existing RTMPose pipeline
- Benchmarking depth-based vs monocular ROM accuracy
- Practical implementation considerations (latency, hardware requirements)

### Out
- General 3D pose estimation unrelated to ROM
- Camera hardware recommendations (hardware already selected)
- Monocular-only approaches

## Assumptions
- Target: shoulder ROM (abduction, flexion, rotation, etc.)
- Current pipeline: RTMPose 2D keypoints + joint angle calculator
- Platform: ARM64 Linux (Jetson/RK3588) or x86
- Real-time requirement: >=15 FPS

## Angles

1. **Orbbec Astra Pro hardware & SDK** — sensor specs, depth technology, OpenNI/OpenNI2 SDK, driver support
2. **Stereo/depth calibration** — intrinsic/extrinsic calibration, depth-to-RGB alignment, distortion correction
3. **Depth-enhanced 2D pose** — using depth to disambiguate left/right, improve keypoint confidence, handle occlusion
4. **2D-to-3D pose lifting** — lifting 2D keypoints to 3D using depth map or point cloud
5. **Point cloud shoulder geometry** — extracting shoulder joint center, head of humerus from point cloud
6. **Hybrid pipeline design** — combining RTMPose 2D + depth data for ROM calculation
7. **Accuracy benchmarks** — published accuracy comparisons: monocular vs stereo/depth for joint angles
8. **Implementation & latency** — practical constraints, compute requirements, real-time feasibility