#include <gtest/gtest.h>

#ifdef OCULUS_HAS_ONNXRUNTIME
#include "oculus/inference/onnx_inference_engine.hpp"
#endif

#include "oculus/core/frame.hpp"

using namespace oculus;

#ifdef OCULUS_HAS_ONNXRUNTIME

TEST(OnnxInferenceEngineTest, DefaultState) {
    OnnxInferenceEngine engine;
    EXPECT_FALSE(engine.is_model_loaded());
    EXPECT_EQ(engine.backend_name(), "cpu");
}

TEST(OnnxInferenceEngineTest, LoadNonexistentModelFails) {
    OnnxInferenceEngine engine;
    EXPECT_FALSE(engine.load_model("nonexistent.onnx"));
    EXPECT_FALSE(engine.is_model_loaded());
}

TEST(OnnxInferenceEngineTest, InferWithoutModelThrows) {
    OnnxInferenceEngine engine;
    Frame frame;
    frame.width = 640;
    frame.height = 480;
    frame.data.resize(640 * 480 * 3);

    EXPECT_THROW(engine.infer(frame), std::runtime_error);
}

#else

TEST(OnnxInferenceEngineTest, SkippedNoOnnxRuntime) {
    GTEST_SKIP() << "ONNX Runtime not available";
}

#endif