// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TRANSPORT_CAST_TRANSPORT_CONFIG_H_
#define MEDIA_CAST_TRANSPORT_CAST_TRANSPORT_CONFIG_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/cast/transport/cast_transport_defines.h"

namespace media {
namespace cast {
namespace transport {

enum RtcpMode {
  kRtcpCompound,  // Compound RTCP mode is described by RFC 4585.
  kRtcpReducedSize,  // Reduced-size RTCP mode is described by RFC 5506.
};

enum VideoCodec {
  kVp8,
  kH264,
};

enum AudioCodec {
  kOpus,
  kPcm16,
  kExternalAudio,
};

struct CastTransportConfig {
  CastTransportConfig();
  ~CastTransportConfig();

  // Transport: Local receiver.
  std::string receiver_ip_address;
  std::string local_ip_address;
  int receive_port;
  int send_port;

  uint32 audio_ssrc;
  uint32 video_ssrc;

  VideoCodec video_codec;
  AudioCodec audio_codec;

  // RTP.
  int audio_rtp_history_ms;
  int video_rtp_history_ms;
  int audio_rtp_max_delay_ms;
  int video_rtp_max_delay_ms;
  int audio_rtp_payload_type;
  int video_rtp_payload_type;

  int audio_frequency;
  int audio_channels;

  std::string aes_key;  // Binary string of size kAesKeySize.
  std::string aes_iv_mask;  // Binary string of size kAesBlockSize.
};

struct EncodedVideoFrame {
  EncodedVideoFrame();
  ~EncodedVideoFrame();

  VideoCodec codec;
  bool key_frame;
  uint32 frame_id;
  uint32 last_referenced_frame_id;
  std::string data;
};

struct EncodedAudioFrame {
  EncodedAudioFrame();
  ~EncodedAudioFrame();

  AudioCodec codec;
  uint32 frame_id;  // Needed to release the frame.
  int samples;  // Needed send side to advance the RTP timestamp.
                // Not used receive side.
  // Support for max sampling rate of 48KHz, 2 channels, 100 ms duration.
  static const int kMaxNumberOfSamples = 48 * 2 * 100;
  std::string data;
};

typedef std::vector<uint8> Packet;
typedef std::vector<Packet> PacketList;

class PacketReceiver : public base::RefCountedThreadSafe<PacketReceiver> {
 public:
  // All packets received from the network should be delivered via this
  // function.
  virtual void ReceivedPacket(const uint8* packet, size_t length,
                              const base::Closure callback) = 0;

  static void DeletePacket(const uint8* packet);

 protected:
  virtual ~PacketReceiver() {}

 private:
  friend class base::RefCountedThreadSafe<PacketReceiver>;
};

class PacketSender {
 public:
  // All packets to be sent to the network will be delivered via these
  // functions.
  virtual bool SendPacket(const transport::Packet& packet) = 0;

  virtual ~PacketSender() {}
};

// Log messages form sender to receiver.
// TODO(mikhal): Refactor to Chromium style (MACRO_STYLE).
enum RtcpSenderFrameStatus {
  kRtcpSenderFrameStatusUnknown = 0,
  kRtcpSenderFrameStatusDroppedByEncoder = 1,
  kRtcpSenderFrameStatusDroppedByFlowControl = 2,
  kRtcpSenderFrameStatusSentToNetwork = 3,
};

struct RtcpSenderFrameLogMessage {
  RtcpSenderFrameLogMessage();
  ~RtcpSenderFrameLogMessage();
  RtcpSenderFrameStatus frame_status;
  uint32 rtp_timestamp;
};

typedef std::list<RtcpSenderFrameLogMessage> RtcpSenderLogMessage;

struct RtcpSenderInfo {
  RtcpSenderInfo();
  ~RtcpSenderInfo();
  // First three members are used for lipsync.
  // First two members are used for rtt.
  uint32 ntp_seconds;
  uint32 ntp_fraction;
  uint32 rtp_timestamp;
  uint32 send_packet_count;
  size_t send_octet_count;
};

struct RtcpReportBlock {
  RtcpReportBlock();
  ~RtcpReportBlock();
  uint32 remote_ssrc;  // SSRC of sender of this report.
  uint32 media_ssrc;  // SSRC of the RTP packet sender.
  uint8 fraction_lost;
  uint32 cumulative_lost;  // 24 bits valid.
  uint32 extended_high_sequence_number;
  uint32 jitter;
  uint32 last_sr;
  uint32 delay_since_last_sr;
};

struct RtcpDlrrReportBlock {
  RtcpDlrrReportBlock();
  ~RtcpDlrrReportBlock();
  uint32 last_rr;
  uint32 delay_since_last_rr;
};

inline bool operator==(RtcpSenderInfo lhs, RtcpSenderInfo rhs) {
  return lhs.ntp_seconds == rhs.ntp_seconds &&
      lhs.ntp_fraction == rhs.ntp_fraction &&
      lhs.rtp_timestamp == rhs.rtp_timestamp &&
      lhs.send_packet_count == rhs.send_packet_count &&
      lhs.send_octet_count == rhs.send_octet_count;
}

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif // MEDIA_CAST_TRANSPORT_CAST_TRANSPORT_CONFIG_H_
