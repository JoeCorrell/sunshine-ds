/**
 * @file src/input.h
 * @brief Declarations for gamepad, keyboard, and mouse input handling.
 */
#pragma once

// standard includes
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

// local includes
#include "platform/common.h"
#include "thread_safe.h"

namespace input {
  struct input_t;

  constexpr std::size_t CLIENT_DISPLAY_COUNT = 2;  ///< Primary and secondary client surfaces supported by Sunshine DS.

  /**
   * @brief Validate a decoded client display index.
   *
   * @param encoded_index Index carried by an absolute mouse or touch packet.
   * @return Array index zero or one, or no value for an unsupported display.
   */
  [[nodiscard]] std::optional<std::size_t> client_display_index(std::uint16_t encoded_index);

  /**
   * @brief Add two input deltas without wrapping the 16-bit wire range.
   *
   * @param first Earlier mouse or wheel delta.
   * @param second Later delta considered for batching.
   * @return Sum when representable, or no value when batching would overflow.
   */
  [[nodiscard]] std::optional<std::int16_t> add_input_delta(std::int16_t first, std::int16_t second);

  /**
   * @brief Write a debug log representation of the input packet.
   *
   * @param input Raw input packet to format for logging.
   */
  void print(void *input);
  /**
   * @brief Reset stream input state after a client disconnect or shutdown.
   *
   * @param input Shared stream input state to reset.
   */
  void reset(std::shared_ptr<input_t> &input);

  /**
   * @brief Queue a raw input message for platform passthrough.
   */
  void passthrough(std::shared_ptr<input_t> &input, std::vector<std::uint8_t> &&input_data);

  /**
   * @brief Initialize global input resources and platform backends.
   *
   * @return Cleanup handle for initialized input resources, or null if none are required.
   */
  [[nodiscard]] std::unique_ptr<platf::deinit_t> init();

  /**
   * @brief Probe whether the platform can create virtual gamepads.
   *
   * @return True when at least one configured gamepad backend is available.
   */
  bool probe_gamepads();

  /**
   * @brief Allocate and initialize platform input state for a stream.
   *
   * @param mail Mailbox used to exchange messages with worker threads.
   * @return Shared input state bound to the stream mailbox.
   */
  std::shared_ptr<input_t> alloc(safe::mail_t mail);

  /**
   * @brief Cancel active contacts and clear coordinates for one client display.
   *
   * @param input Session input state.
   * @param display_index Zero-based client display index.
   */
  void cancel_display_touches(std::shared_ptr<input_t> &input, std::size_t display_index);

  /**
   * @brief Touchscreen coordinate bounds used to scale absolute input.
   */
  struct touch_port_t: public platf::touch_port_t {
    int env_width;  ///< Width of the full capture environment in physical pixels.
    int env_height;  ///< Height of the full capture environment in physical pixels.

    // Offset x and y coordinates of the client
    float client_offsetX;  ///< Horizontal client viewport offset used when scaling touch input.
    float client_offsetY;  ///< Vertical client viewport offset used when scaling touch input.

    float scalar_inv;  ///< Inverse scale factor from client coordinates to display coordinates.
    float scalar_tpcoords;  ///< Scale factor from client coordinates to touch-port coordinates.

    int env_logical_width;  ///< Width of the full capture environment after display scaling.
    int env_logical_height;  ///< Height of the full capture environment after display scaling.

    /**
     * @brief Check whether the touch-port bounds are initialized.
     */
    explicit operator bool() const {
      return width != 0 && height != 0 && env_width != 0 && env_height != 0;
    }
  };

  /**
   * @brief Normalize desktop coordinates into one monitor's rendered content.
   *
   * Letterbox/pillarbox bars are excluded before deriving logical monitor
   * dimensions, so a non-matching capture aspect ratio still spans the complete
   * Windows monitor.
   *
   * @param touch_port Current capture and desktop geometry.
   * @param coords In/out desktop coordinate pair.
   * @return Monitor-local logical touch port, or no value for invalid geometry.
   */
  [[nodiscard]] std::optional<platf::touch_port_t> monitor_touch_port(
    const input::touch_port_t &touch_port,
    std::pair<float, float> &coords
  );

  /**
   * @brief Scale the ellipse axes according to the provided size.
   * @param val The major and minor axis pair.
   * @param rotation The rotation value from the touch/pen event.
   * @param scalar The scalar cartesian coordinate pair.
   * @return The major and minor axis pair.
   */
  std::pair<float, float> scale_client_contact_area(const std::pair<float, float> &val, uint16_t rotation, const std::pair<float, float> &scalar);
}  // namespace input
