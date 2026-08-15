/**
 * @file src/stream.h
 * @brief Declarations for the streaming protocols.
 */
#pragma once

// standard includes
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

// lib includes
#include <boost/asio.hpp>

// local includes
#include "audio.h"
#include "crypto.h"
#include "video.h"

namespace stream {
  constexpr auto VIDEO_STREAM_PORT = 9;  ///< GameStream base-port offset used for the video UDP stream.
  constexpr auto CONTROL_PORT = 10;  ///< GameStream base-port offset used for the control channel.
  constexpr auto AUDIO_STREAM_PORT = 11;  ///< GameStream base-port offset used for the audio UDP stream.

  /**
   * @brief Base-port offset for the second display's video UDP stream.
   *
   * 12 because offsets 0, 9, 10, 11 and 21 are taken and 12 sits next to audio,
   * which keeps the whole AV group contiguous for anyone reading a firewall rule.
   *
   * This is an extension: no GameStream client asks for it, and none is offered
   * it unless it says it wants one. See `docs/dual_display_protocol.md`.
   */
  constexpr auto VIDEO_STREAM_2_PORT = 12;

  /**
   * @brief Type-erased lifetime token that keeps the broadcast sockets bound.
   *
   * RTSP negotiates UDP ports before ANNOUNCE creates a stream session. Keeping
   * this token on the launch session closes that gap, so a successful SETUP for
   * video stream 1 reserves its socket until the session takes ownership.
   */
  using port_reservation_t = std::shared_ptr<void>;

  /**
   * @brief Bind and reserve all broadcast sockets needed by video stream 1.
   *
   * @return A reservation while the second video socket is ready, or an empty
   * token if the socket could not be opened or bound. Destroying the final token
   * releases the sockets when no active stream session retains them.
   */
  [[nodiscard]] port_reservation_t reserve_second_video_port();

  /**
   * @brief Probe whether the optional UDP video sender is currently ready.
   *
   * This is a lightweight bind probe with no broadcaster or worker-thread side
   * effects. SETUP still acquires a real lifetime reservation and verifies the
   * sender because readiness can change after `/serverinfo`.
   *
   * @return True when video stream 1 can presently be reserved.
   */
  [[nodiscard]] bool second_video_port_available();

  /**
   * @brief Decode the video-stream index carried by an IDR control request.
   *
   * Stock clients send two zero bytes. Dual-display clients use the formerly
   * reserved first byte for the stream index and retain a zero second byte.
   * @param payload Raw IDR request payload from the control channel.
   * @return Stream index zero or one, or no value for a malformed request.
   */
  [[nodiscard]] std::optional<std::uint8_t> idr_stream_index(std::string_view payload);

  /**
   * @brief Parsed reference-frame invalidation control request.
   */
  struct ref_frame_invalidation_t {
    std::int64_t first_frame;  ///< First frame in the invalidated range.
    std::int64_t last_frame;  ///< Last frame in the invalidated range.
    std::uint8_t stream_index;  ///< Video stream receiving the invalidation.
  };

  /**
   * @brief Decode an exact 24-byte reference-frame invalidation request.
   *
   * The third formerly reserved 64-bit word carries the stream index. Stock
   * packets contain zero and therefore continue to target the primary stream.
   *
   * @param payload Raw control-channel payload.
   * @return Parsed request, or no value for a malformed size or index.
   */
  [[nodiscard]] std::optional<ref_frame_invalidation_t> parse_ref_frame_invalidation(std::string_view payload);

  struct session_t;

  /**
   * @brief Stream configuration shared by capture and network senders.
   */
  struct config_t {
    audio::config_t audio;  ///< Audio capture configuration for the stream.
    video::config_t monitor;  ///< Video capture and encoder configuration for the selected monitor.

    /**
     * @brief The second display, when the client asked for one.
     *
     * Empty for every client that did not, which is all of them until one is
     * built against `docs/dual_display_protocol.md`. Optional rather than a
     * flag beside a always-present struct, so there is no way to read a second
     * monitor's configuration without having established that there is one.
     *
     * Budgeted separately from [monitor] rather than sharing its bitrate. The
     * second panel usually holds a desktop that is static for minutes at a
     * time, and splitting one budget by area would starve the game to reserve
     * bandwidth for a screen that is not changing.
     */
    std::optional<video::config_t> monitor2;

    int packetsize;  ///< Maximum payload size for network packets.
    int minRequiredFecPackets;  ///< Minimum recovery packets required before FEC is emitted.
    int mlFeatureFlags;  ///< Moonlight feature flags negotiated for this session.
    int controlProtocolType;  ///< GameStream control protocol variant selected by the client.
    int audioQosType;  ///< Audio QoS type.
    int videoQosType;  ///< Video QoS type.

    uint32_t encryptionFlagsEnabled;  ///< Bitmask of GameStream encryption features enabled for the session.

    std::optional<int> gcmap;  ///< Optional game-controller mapping override from the launch request.
  };

  namespace session {
    /**
     * @brief Enumerates supported state options.
     */
    enum class state_e : int {
      STOPPED,  ///< The session is stopped
      STOPPING,  ///< The session is stopping
      STARTING,  ///< The session is starting
      RUNNING,  ///< The session is running
    };

    /**
     * @brief Allocate and initialize platform input state for a stream.
     *
     * @param config Configuration values to apply.
     * @param launch_session Launch session.
     * @return Allocated object or identifier, or an error value on failure.
     */
    std::shared_ptr<session_t> alloc(config_t &config, rtsp_stream::launch_session_t &launch_session);
    /**
     * @brief Start a streaming session for the supplied peer address.
     *
     * @param session Active streaming or pairing session for the request.
     * @param addr_string Addr string.
     * @return Start status.
     */
    int start(session_t &session, const std::string &addr_string);
    /**
     * @brief Stop a streaming session and prevent more packets from being queued.
     *
     * @param session Active streaming or pairing session for the request.
     */
    void stop(session_t &session);
    /**
     * @brief Wait for worker threads owned by the session to exit.
     *
     * @param session Active streaming or pairing session for the request.
     */
    void join(session_t &session);
    /**
     * @brief Platform handle returned from stream setup.
     *
     * @param session Active streaming or pairing session for the request.
     * @return Current lifecycle state for the stream session.
     */
    state_e state(session_t &session);
    /**
     * @brief Return the paired client certificate for a stream session.
     *
     * @param session Active streaming or pairing session for the request.
     * @return PEM certificate associated with the session's client.
     */
    const std::string &client_cert(session_t &session);
  }  // namespace session
}  // namespace stream
