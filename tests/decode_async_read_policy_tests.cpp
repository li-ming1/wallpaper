#include "wallpaper/decode_async_read_policy.h"

#include "test_support.h"

TEST_CASE(DecodeAsyncReadPolicy_StartPrimesSingleReadRequest) {
  wallpaper::DecodeAsyncReadState state;

  wallpaper::ResumeDecodeAsyncRead(&state);
  EXPECT_EQ(wallpaper::PeekDecodeAsyncReadAction(state),
            wallpaper::DecodeAsyncReadAction::kIssueRead);

  wallpaper::MarkDecodeAsyncReadIssued(&state);
  EXPECT_EQ(wallpaper::PeekDecodeAsyncReadAction(state),
            wallpaper::DecodeAsyncReadAction::kNone);
}

TEST_CASE(DecodeAsyncReadPolicy_SampleCompletionWaitsForConsumptionBeforeReissue) {
  wallpaper::DecodeAsyncReadState state;

  wallpaper::ResumeDecodeAsyncRead(&state);
  wallpaper::MarkDecodeAsyncReadIssued(&state);
  wallpaper::MarkDecodeAsyncReadCompleted(true, false, &state);
  EXPECT_EQ(wallpaper::PeekDecodeAsyncReadAction(state),
            wallpaper::DecodeAsyncReadAction::kNone);

  wallpaper::MarkDecodeAsyncReadSampleConsumed(&state);
  EXPECT_EQ(wallpaper::PeekDecodeAsyncReadAction(state),
            wallpaper::DecodeAsyncReadAction::kIssueRead);
}

TEST_CASE(DecodeAsyncReadPolicy_PauseStopsReissueAfterConsumption) {
  wallpaper::DecodeAsyncReadState state;

  wallpaper::ResumeDecodeAsyncRead(&state);
  wallpaper::MarkDecodeAsyncReadIssued(&state);
  wallpaper::MarkDecodeAsyncReadCompleted(true, false, &state);
  wallpaper::PauseDecodeAsyncRead(&state);
  wallpaper::MarkDecodeAsyncReadSampleConsumed(&state);

  EXPECT_EQ(wallpaper::PeekDecodeAsyncReadAction(state),
            wallpaper::DecodeAsyncReadAction::kNone);
}

TEST_CASE(DecodeAsyncReadPolicy_ResetClearsPendingAndReadyState) {
  wallpaper::DecodeAsyncReadState state;

  wallpaper::ResumeDecodeAsyncRead(&state);
  wallpaper::MarkDecodeAsyncReadIssued(&state);
  wallpaper::MarkDecodeAsyncReadCompleted(true, false, &state);
  wallpaper::ResetDecodeAsyncRead(&state);

  EXPECT_TRUE(!state.running);
  EXPECT_TRUE(!state.readPending);
  EXPECT_TRUE(!state.sampleReady);
  EXPECT_TRUE(!state.seekToStartPending);
  EXPECT_EQ(wallpaper::PeekDecodeAsyncReadAction(state),
            wallpaper::DecodeAsyncReadAction::kNone);
}

TEST_CASE(DecodeAsyncReadPolicy_EndOfStreamRequestsSeekBeforeRead) {
  wallpaper::DecodeAsyncReadState state;

  wallpaper::ResumeDecodeAsyncRead(&state);
  wallpaper::MarkDecodeAsyncReadIssued(&state);
  wallpaper::MarkDecodeAsyncReadCompleted(false, true, &state);
  EXPECT_EQ(wallpaper::PeekDecodeAsyncReadAction(state),
            wallpaper::DecodeAsyncReadAction::kSeekToStart);

  wallpaper::MarkDecodeAsyncReadSeekCompleted(&state);
  EXPECT_EQ(wallpaper::PeekDecodeAsyncReadAction(state),
            wallpaper::DecodeAsyncReadAction::kIssueRead);
}

TEST_CASE(DecodeAsyncReadPolicy_DoesNotPrefetchImmediatelyAfterConsume) {
  EXPECT_TRUE(!wallpaper::ShouldIssueReadImmediatelyAfterConsume());
}
