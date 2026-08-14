/**
 * @file src/dual_display.cpp
 * @brief Definitions for second-display streaming.
 */
// local includes
#include "dual_display.h"

#include "config.h"
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
     * @brief Whether an indirect display driver is installed and usable.
     *
     * Windows has no API to create a monitor. A virtual display is an IddCx
     * driver — signed, installed by the user, and outside this program — so all
     * that can be done here is find one and ask it.
     *
     * Unimplemented, and reporting false is therefore correct rather than a
     * placeholder: with no driver bound, `supported()` says no, `/serverinfo`
     * advertises one video stream, `SETUP` for `video/1/0` is refused, and every
     * client behaves exactly as it does against stock Sunshine. The feature is
     * off, not broken.
     *
     * @todo Bind to an installed IddCx driver. See `docs/virtual_display.md`.
     */
    [[nodiscard]] bool virtual_display_available() {
      return false;
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
      // Unreachable while `virtual_display_available()` reports false; kept so
      // that binding a driver is a change in one place.
      BOOST_LOG(warning) << "Virtual display requested but no driver is bound"sv;
      return nullptr;
    }

    BOOST_LOG(info) << "Second display: capturing "sv << source << " at "sv
                    << request.width << 'x' << request.height << '@' << request.framerate;
    return std::make_unique<physical_lease_t>(source, request);
  }

}  // namespace dual_display
