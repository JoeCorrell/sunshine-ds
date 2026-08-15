/**
 * @file src/dual_display.cpp
 * @brief Definitions for second-display streaming.
 */
// local includes
#include "dual_display.h"

#include "config.h"
#include "display_device.h"
#include "logging.h"
#include "platform/common.h"

// standard includes
#include <algorithm>

using namespace std::literals;

namespace dual_display {

  namespace {

    /**
     * @brief The configured value that selects the virtual display.
     *
     * A reserved word rather than a separate boolean, so the setting reads as
     * one question — "where does the second display come from" — with one
     * answer. Two settings would allow the contradictory state of a named
     * monitor with the virtual flag also set, which somebody would then have to
     * decide the meaning of.
     */
    constexpr auto VIRTUAL = "virtual"sv;

    /**
     * @brief A real monitor that already exists.
     *
     * Nothing is created and nothing is destroyed; the lease is a name and a
     * mode. The mode is what the client asked for rather than what the monitor
     * is, because the encoder scales to the requested size and the alternative —
     * querying the monitor and streaming its native mode — sends a 4K desktop to
     * a panel that cannot show it.
     */
    class physical_lease_t: public lease_t {
    public:
      physical_lease_t(std::string name, const request_t &request):
          m_name {std::move(name)},
          m_granted {request} {
      }

      [[nodiscard]] std::string output_name() const override {
        return m_name;
      }

      [[nodiscard]] request_t granted() const override {
        return m_granted;
      }

    private:
      std::string m_name;
      request_t m_granted;
    };

    /**
     * @brief The display the game is being streamed from.
     *
     * The second display is defined as "not this one", so this is what it is
     * measured against. Empty means the platform default, which is also what an
     * empty `output_name` means to the capture backend.
     */
    [[nodiscard]] std::string primary_output() {
      return display_device::map_output_name(config::video.output_name);
    }

    /**
     * @brief A display to stream that is not the one carrying the game.
     *
     * Windows has no API to *create* a monitor -- a virtual display is an IddCx
     * driver, signed and installed by the user, outside this program entirely.
     * What this does instead is find one that is already attached.
     *
     * That is deliberately not the same as "create on demand", and the
     * difference is worth stating: the monitor has to already exist, which for a
     * virtual display driver means its monitor is switched on. In exchange it
     * needs no driver-specific control channel, works with any IddCx driver
     * rather than one this fork was written against, and needs no elevation --
     * a driver-specific implementation would need all three.
     *
     * Returns empty when there is nothing but the game's own display, which is
     * the ordinary single-monitor case.
     */
    [[nodiscard]] std::string find_spare_output() {
      const auto primary = primary_output();
      const auto outputs = platf::display_names(platf::mem_type_e::system);

      for (const auto &output : outputs) {
        if (output != primary) {
          return output;
        }
      }

      // A single display, or the only other one is the game's. Either way there
      // is nothing here to give.
      return {};
    }

    /**
     * @brief Whether a second display can be provided without being named.
     */
    [[nodiscard]] bool virtual_display_available() {
      return !find_spare_output().empty();
    }

  }  // namespace

  bool supported() {
    const auto &source = config::video.dual_display_source;

    if (source.empty()) {
      return false;
    }

    if (source == VIRTUAL) {
      return virtual_display_available();
    }

    // A named monitor, which must actually be attached. Checked rather than
    // trusted: a name left in the config after the monitor was unplugged would
    // otherwise advertise a capability that fails at capture, and a second
    // stream that opens and never carries a frame is worse for the client than
    // one that was never offered.
    const auto outputs = platf::display_names(platf::mem_type_e::system);
    return std::find(std::begin(outputs), std::end(outputs), source) != std::end(outputs);
  }

  std::unique_ptr<lease_t> acquire(const request_t &request) {
    if (!supported()) {
      return nullptr;
    }

    const auto &source = config::video.dual_display_source;

    if (source == VIRTUAL) {
      const auto spare = find_spare_output();
      if (spare.empty()) {
        BOOST_LOG(warning) << "No spare display to serve as a second display"sv;
        return nullptr;
      }

      BOOST_LOG(info) << "Second display: capturing spare output "sv << spare << " at "sv
                      << request.width << 'x' << request.height << '@' << request.framerate;
      return std::make_unique<physical_lease_t>(spare, request);
    }

    BOOST_LOG(info) << "Second display: capturing "sv << source << " at "sv
                    << request.width << 'x' << request.height << '@' << request.framerate;
    return std::make_unique<physical_lease_t>(source, request);
  }

}  // namespace dual_display
