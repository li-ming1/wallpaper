#pragma once

namespace wallpaper {

enum class DecodeAsyncReadAction {
  kNone = 0,
  kIssueRead = 1,
  kSeekToStart = 2,
};

struct DecodeAsyncReadState final {
  bool running = false;
  bool readPending = false;
  bool sampleReady = false;
  bool seekToStartPending = false;
};

void ResumeDecodeAsyncRead(DecodeAsyncReadState* state);
void PauseDecodeAsyncRead(DecodeAsyncReadState* state);
void ResetDecodeAsyncRead(DecodeAsyncReadState* state);
DecodeAsyncReadAction PeekDecodeAsyncReadAction(const DecodeAsyncReadState& state);
void MarkDecodeAsyncReadIssued(DecodeAsyncReadState* state);
void MarkDecodeAsyncReadCompleted(bool hasSample, bool endOfStream, DecodeAsyncReadState* state);
void MarkDecodeAsyncReadSampleConsumed(DecodeAsyncReadState* state);
void MarkDecodeAsyncReadSeekCompleted(DecodeAsyncReadState* state);

}  // namespace wallpaper
