/**
 * @file src/dual_display.h
 * @brief Declarations for second-display streaming.
 *
 * A client with two screens can be sent two displays at once rather than one
 * display and a trackpad. The wire format is described in
 * `docs/dual_display_protocol.md`; this header covers the half that decides
 * *what* the second display is.
 */
#pragma once

// standard includes
#include <memory>
#include <string>
#include <string_view>

namespace dual_display {

  /**
   * @brief What a client asked its second screen to be.
   */
  struct request_t {
    int width;  ///< Width in pixels of the client's second panel.
    int height;  ///< Height in pixels of the client's second panel.
    int framerate;  ///< Requested frames per second for the second stream.
    std::string_view client_id {};  ///< Stable paired-client identity used for persistent virtual-monitor topology.
  };

  /**
   * @brief Validate a requested second-display mode.
   *
   * @param request Mode supplied by the remote client.
   * @return True when every dimension is positive and representable by the
   * SudoVDA control protocol.
   */
  [[nodiscard]] bool valid_request(const request_t &request);

  /**
   * @brief A display being streamed as the second video stream.
   *
   * Held for the life of a session. Destroying it releases whatever was
   * acquired — a virtual monitor is removed, a real one is simply forgotten.
   */
  class lease_t {
  public:
    virtual ~lease_t() = default;

    /**
     * @brief The name capture should be opened against.
     *
     * The same identifier `config::video.output_name` carries, so the existing
     * capture path takes it unchanged and neither knows nor cares whether the
     * display behind it is physical.
     */
    [[nodiscard]] virtual std::string output_name() const = 0;

  };

  /**
   * @brief Whether this host can serve a second display at all.
   *
   * Consulted before the capability is advertised in `/serverinfo` and before
   * an `RTSP SETUP` for `streamid=video/1/0` is answered with a port of its own.
   * Both must agree: a host that advertises the capability and then refuses the
   * setup leaves a client holding a stream that will never carry a frame.
   *
   * Host-global rather than per-session on purpose. `SETUP` arrives *before*
   * `ANNOUNCE`, so at the moment a port must be chosen the server has not yet
   * seen the client's SDP and cannot know what this particular session wants.
   */
  [[nodiscard]] bool supported();

  /**
   * @brief Acquire a display to stream as the second video stream.
   *
   * @param request What the client asked for.
   * @return The lease, or empty when no second display could be provided.
   *
   * Failure is ordinary and must not be fatal: the driver may be absent, the
   * user may have removed it, the mode may be refused. The session then runs
   * with one display, which is what every session does today.
   */
  [[nodiscard]] std::unique_ptr<lease_t> acquire(const request_t &request);

}  // namespace dual_display
