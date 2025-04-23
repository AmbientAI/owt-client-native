#ifndef OWT_BASE_ENCODEDFRAMEGENERATORINTERFACE_H_
#define OWT_BASE_ENCODEDFRAMEGENERATORINTERFACE_H_

// #include "api/video/video_frame_buffer.h"
// #include "rtc_base/checks.h"
// #include "webrtc/api/scoped_refptr.h"

namespace owt {
namespace base {


struct EncodedVideoFrameBufferNativeHandle{
  bool is_keyframe = false;
  size_t width = 0;
  size_t height = 0;
  std::vector<uint8_t> data;
};


// class EncodedVideoFrameBuffer : public webrtc::VideoFrameBuffer {
//  public:
//   EncodedVideoFrameBuffer(std::unique_ptr<EncodedVideoFrameBufferNativeHandle> native_handle)
//     : native_handle_(std::move(native_handle))
//     , width_(native_handle_ ? native_handle_->width : 0)
//     , height_(native_handle_ ? native_handle_->height : 0)
//   {}
//   ~EncodedVideoFrameBuffer() override {}
//   webrtc::VideoFrameBuffer::Type type() const override { return webrtc::VideoFrameBuffer::Type::kNative; }
//   int width() const override { return width_; }
//   int height() const override { return height_; }
//   rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override {
//     RTC_NOTREACHED();
//     return nullptr;
//   }
//   EncodedVideoFrameBufferNativeHandle* native_handle() { return native_handle_.get(); }
//  private:
//   std::unique_ptr<EncodedVideoFrameBufferNativeHandle> native_handle_;
//   size_t width_;
//   size_t height_;
// };


/* Interface for generating encoded frames, to be used with a "passthrough
 * encoder". This is the analogue of the VideoFrameGeneratorInterface.
 */
class EncodedVideoFrameGeneratorInterface {
 public:
  virtual ~EncodedVideoFrameGeneratorInterface() {}

  virtual std::unique_ptr<EncodedVideoFrameBufferNativeHandle> GenerateNextEncodedFrame() = 0;

  /**
  @brief This function can perform any cleanup that must be done on the same thread as
  GenerateNextFrame(). Default implementation provided for backwards compatibility.
  */
  virtual void Cleanup() {}
};


} // namespace base
} // namespace owt

#endif  // OWT_BASE_ENCODEDFRAMEGENERATORINTERFACE_H_
