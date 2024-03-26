// MIT-licensed, see LICENSE in the root directory.

#include <array>
#include <libretro.h>

struct MachineState
{
    retro_time_t elapsed {};
};

namespace
{
    retro_video_refresh_t _video_refresh = nullptr;
    retro_environment_t _environment = nullptr;
    retro_log_printf_t _log = nullptr;

    static constexpr std::array ERRORS = {
        "NO_FAULT_FOUND",
        "BEATER OVERLOAD",
        "HPCO COMPRESSOR",
        "PRODUCT DOOR OFF",
        "HOPPER THERMISTOR FAIL / BAD",
        "BARREL THERMISTOR FAIL / BAD",
    };
}

RETRO_API void retro_set_video_refresh(retro_video_refresh_t refresh)
{
    _video_refresh = refresh;
}

RETRO_API void retro_set_audio_sample(retro_audio_sample_t);
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t);
RETRO_API void retro_set_input_poll(retro_input_poll_t);
RETRO_API void retro_set_input_state(retro_input_state_t);

RETRO_API void retro_set_environment(retro_environment_t env)
{
    _environment = env;
    retro_log_callback log = { .log = nullptr };
    bool yes = true;
    _environment(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &yes);
    _environment(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log);
    _log = log.log;
}

/* Library global initialization/deinitialization. */
RETRO_API void retro_init()
{
}

RETRO_API void retro_deinit()
{
}


RETRO_API unsigned retro_api_version()
{
    return RETRO_API_VERSION;
}


RETRO_API void retro_get_system_info(retro_system_info *info)
{
    info->library_name = "McTaylor";
    info->block_extract = false;
}


RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info)
{
    info->geometry.max_height = 640;
    info->geometry.max_width = 480;
    info->geometry.base_height = 640;
    info->geometry.base_width = 480;
    info->timing.fps = 60.0;
    info->timing.sample_rate = 44100.0;
}

RETRO_API void retro_set_controller_port_device(unsigned, unsigned) {}

RETRO_API void retro_reset(void)
{
    // TODO: Reset the game
    // TODO: Random probability of failure
}

/* Runs the game for one video frame.
 * During retro_run(), input_poll callback must be called at least once.
 *
 * If a frame is not rendered for reasons where a game "dropped" a frame,
 * this still counts as a frame, and retro_run() should explicitly dupe
 * a frame if GET_CAN_DUPE returns true.
 * In this case, the video callback can take a NULL argument for data.
 */
RETRO_API void retro_run()
{
    // TODO: Render the background
}

RETRO_API size_t retro_serialize_size(void)
{
    return 0;
}

/* Serializes internal state. If failed, or size is lower than
 * retro_serialize_size(), it should return false, true otherwise. */
RETRO_API bool retro_serialize(void *data, size_t size);
RETRO_API bool retro_unserialize(const void *data, size_t size);

RETRO_API void retro_cheat_reset() {}
RETRO_API void retro_cheat_set(unsigned, bool, const char *) {}

/* Loads a game.
 * Return true to indicate successful loading and false to indicate load failure.
 */
RETRO_API bool retro_load_game(const struct retro_game_info *game)
{
    // TODO: Load the background
    return true;
}

RETRO_API bool retro_load_game_special(unsigned, const retro_game_info *info, size_t)
{
    return retro_load_game(info);
}

/* Unloads the currently loaded game. Called before retro_deinit(void). */
RETRO_API void retro_unload_game()
{
}

RETRO_API unsigned retro_get_region() { return RETRO_REGION_NTSC; }

/* Gets region of memory. */
RETRO_API void *retro_get_memory_data(unsigned id)
{
    return nullptr;
}

RETRO_API size_t retro_get_memory_size(unsigned id)
{
    return 0;
}
