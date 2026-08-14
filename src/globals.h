/**
 * @file globals.h
 * @brief Declarations for globally accessible variables and functions.
 */
#pragma once

// local includes
#include "entry_handler.h"
#include "thread_pool.h"

/**
 * @brief A thread pool for processing tasks.
 */
extern thread_pool_util::ThreadPool task_pool;

/**
 * @brief A boolean flag to indicate whether the cursor should be displayed.
 */
extern bool display_cursor;

#ifdef _WIN32
  // Declare global singleton used for NVIDIA control panel modifications
  #include "platform/windows/nvprefs/nvprefs_interface.h"

/**
 * @brief A global singleton used for NVIDIA control panel modifications.
 */
extern nvprefs::nvprefs_interface nvprefs_instance;
#endif

/**
 * @brief Handles process-wide communication.
 */
namespace mail {
/**
 * @def MAIL(x)
 * @brief Macro for MAIL.
 */
#define MAIL(x) \
  constexpr auto x = std::string_view { \
    #x \
  }

  /**
   * @brief A process-wide communication mechanism.
   */
  extern safe::mail_t man;

  // Global mail
  MAIL(shutdown);  ///< Shutdown.
  MAIL(broadcast_shutdown);  ///< Broadcast shutdown.
  MAIL(video_packets);  ///< Video packets.
  /**
   * Video packets for the second display.
   *
   * A queue of its own rather than a tagged stream index on the first. The two
   * encoders run at different rates and the sender threads drain independently,
   * so one queue would couple them: a slow desktop frame would sit in front of
   * the game frame behind it, and the screen being played on would wait for the
   * screen nobody is looking at.
   */
  MAIL(video_packets2);
  MAIL(audio_packets);  ///< Audio packets.
  MAIL(switch_display);  ///< Switch display.

  // Local mail
  MAIL(touch_port);  ///< Touch port.
  MAIL(idr);  ///< IDR.
  MAIL(invalidate_ref_frames);  ///< Invalidate ref frames.
  MAIL(gamepad_feedback);  ///< Gamepad feedback.
  MAIL(hdr);  ///< HDR.
#undef MAIL

}  // namespace mail
