#include "wallpaper/decode_async_read_policy.h"

namespace wallpaper {

void ResumeDecodeAsyncRead(DecodeAsyncReadState* state) {
  if (state == nullptr) {
    return;
  }
  state->running = true;
}

void PauseDecodeAsyncRead(DecodeAsyncReadState* state) {
  if (state == nullptr) {
    return;
  }
  state->running = false;
}

void ResetDecodeAsyncRead(DecodeAsyncReadState* state) {
  if (state == nullptr) {
    return;
  }
  *state = DecodeAsyncReadState{};
}

DecodeAsyncReadAction PeekDecodeAsyncReadAction(const DecodeAsyncReadState& state) {
  if (!state.running) {
    return DecodeAsyncReadAction::kNone;
  }
  if (!state.sampleReady && !state.readPending && state.seekToStartPending) {
    return DecodeAsyncReadAction::kSeekToStart;
  }
  if (!state.sampleReady && !state.readPending) {
    return DecodeAsyncReadAction::kIssueRead;
  }
  return DecodeAsyncReadAction::kNone;
}

void MarkDecodeAsyncReadIssued(DecodeAsyncReadState* state) {
  if (state == nullptr || !state->running || state->sampleReady || state->readPending ||
      state->seekToStartPending) {
    return;
  }
  state->readPending = true;
}

void MarkDecodeAsyncReadCompleted(const bool hasSample, const bool endOfStream,
                                  DecodeAsyncReadState* state) {
  if (state == nullptr) {
    return;
  }
  state->readPending = false;
  if (endOfStream) {
    state->sampleReady = false;
    state->seekToStartPending = true;
    return;
  }
  state->sampleReady = hasSample;
}

void MarkDecodeAsyncReadSampleConsumed(DecodeAsyncReadState* state) {
  if (state == nullptr) {
    return;
  }
  state->sampleReady = false;
}

void MarkDecodeAsyncReadSeekCompleted(DecodeAsyncReadState* state) {
  if (state == nullptr) {
    return;
  }
  state->seekToStartPending = false;
}

}  // namespace wallpaper
