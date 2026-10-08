#include <gtest/gtest.h>
#include "oculus/rom/session_manager.hpp"

using namespace oculus;

TEST(SessionManagerTest, StartStopSession) {
    SessionManager mgr;
    EXPECT_FALSE(mgr.is_active());

    mgr.start_session("test", AnalysisMode::REALTIME);
    EXPECT_TRUE(mgr.is_active());
    EXPECT_TRUE(mgr.is_recording());

    mgr.stop_session();
    EXPECT_FALSE(mgr.is_active());
}

TEST(SessionManagerTest, RecordFrames) {
    SessionManager mgr;
    mgr.start_session("test", AnalysisMode::RECORD);

    PoseResult pose;
    pose.timestamp = 100;
    mgr.record_frame(pose);
    pose.timestamp = 200;
    mgr.record_frame(pose);

    auto session = mgr.get_current_session();
    EXPECT_EQ(session.poses.size(), 2u);
}

TEST(SessionManagerTest, SessionsList) {
    SessionManager mgr;
    mgr.start_session("s1", AnalysisMode::REALTIME);
    mgr.stop_session();
    mgr.start_session("s2", AnalysisMode::RECORD);
    mgr.stop_session();

    EXPECT_EQ(mgr.list_sessions().size(), 2u);
}

TEST(SessionManagerTest, StopSessionWithoutStart) {
    SessionManager mgr;
    mgr.stop_session();
    EXPECT_FALSE(mgr.is_active());
}