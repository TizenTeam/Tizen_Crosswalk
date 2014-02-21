// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/cast_transport_sender_impl.h"

#include "base/task_runner.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/cast_transport_defines.h"

namespace media {
namespace cast {
namespace transport {

CastTransportSender* CastTransportSender::CreateCastTransportSender(
    base::TickClock* clock,
    const CastTransportConfig& config,
    const CastTransportStatusCallback& status_callback,
    const scoped_refptr<base::TaskRunner>& transport_task_runner) {
  return new CastTransportSenderImpl(clock, config, status_callback,
                                     transport_task_runner.get());
}

CastTransportSenderImpl::CastTransportSenderImpl(
    base::TickClock* clock,
    const CastTransportConfig& config,
    const CastTransportStatusCallback& status_callback,
    const scoped_refptr<base::TaskRunner>& transport_task_runner)
    : pacer_(clock, &config, NULL, transport_task_runner, status_callback),
      rtcp_builder_(&pacer_),
      audio_sender_(config, clock, &pacer_),
      video_sender_(config, clock, &pacer_) {
  if (audio_sender_.initialized() && video_sender_.initialized()) {
    status_callback.Run(TRANSPORT_INITIALIZED);
  } else {
    status_callback.Run(TRANSPORT_UNINITIALIZED);
  }
}

CastTransportSenderImpl::~CastTransportSenderImpl() {
}

void CastTransportSenderImpl::SetPacketReceiver(
    scoped_refptr<PacketReceiver> packet_receiver) {
  pacer_.SetPacketReceiver(packet_receiver);
}

void CastTransportSenderImpl::InsertCodedAudioFrame(
    const EncodedAudioFrame* audio_frame,
    const base::TimeTicks& recorded_time) {
  audio_sender_.InsertCodedAudioFrame(audio_frame, recorded_time);
}

void CastTransportSenderImpl::InsertCodedVideoFrame(
    const EncodedVideoFrame* video_frame,
    const base::TimeTicks& capture_time) {
  video_sender_.InsertCodedVideoFrame(video_frame, capture_time);
}

void CastTransportSenderImpl::SendRtcpFromRtpSender(
    uint32 packet_type_flags,
    const RtcpSenderInfo& sender_info,
    const RtcpDlrrReportBlock& dlrr,
    const RtcpSenderLogMessage& sender_log,
    uint32 sending_ssrc,
    const std::string& c_name) {
  rtcp_builder_.SendRtcpFromRtpSender(packet_type_flags,
                                      sender_info,
                                      dlrr,
                                      sender_log,
                                      sending_ssrc,
                                      c_name);
}

void CastTransportSenderImpl::ResendPackets(
    bool is_audio, const MissingFramesAndPacketsMap& missing_packets) {
  if (is_audio) {
    audio_sender_.ResendPackets(missing_packets);
  } else {
    video_sender_.ResendPackets(missing_packets);
  }
}

void CastTransportSenderImpl::RtpAudioStatistics(
    const base::TimeTicks& now,
    RtcpSenderInfo* sender_info) {
  audio_sender_.GetStatistics(now, sender_info);
}

void CastTransportSenderImpl::RtpVideoStatistics(
    const base::TimeTicks& now,
    RtcpSenderInfo* sender_info) {
  video_sender_.GetStatistics(now, sender_info);
}

void CastTransportSenderImpl::InsertFakeTransportForTesting(
    PacketSender* fake_transport) {
  pacer_.InsertFakeTransportForTesting(fake_transport);
}

}  // namespace transport
}  // namespace cast
}  // namespace media
