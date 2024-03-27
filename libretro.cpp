// MIT-licensed, see LICENSE in the root directory.

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>

#include <libretro.h>
#include <retro_assert.h>
#include <audio/audio_mixer.h>
#include <audio/conversion/float_to_s16.h>
#include <formats/rwav.h>

#include <embedded/mctaylor_freezer.h>

using std::array;

constexpr int SAMPLE_RATE = 44100;
constexpr int SCREEN_WIDTH = 1366;
constexpr int SCREEN_HEIGHT = 768;

struct MachineState
{
    retro_time_t elapsed {};
};

struct CoreState
{
    CoreState() noexcept
    {
        audio_mixer_init(SAMPLE_RATE);
        _freezerSound = audio_mixer_load_wav(
            (void*)embedded_mctaylor_freezer,
            sizeof(embedded_mctaylor_freezer),
            "sinc",
            RESAMPLER_QUALITY_HIGHEST
        );

        retro_assert(_freezerSound != nullptr);

        _freezerVoice = audio_mixer_play(_freezerSound, true, 1.0f, "sinc", RESAMPLER_QUALITY_HIGHEST, nullptr);

        retro_assert(_freezerVoice != nullptr);
    }

    ~CoreState() noexcept
    {
        audio_mixer_stop(_freezerVoice);
        _freezerVoice = nullptr;

        audio_mixer_destroy(_freezerSound);
        _freezerSound = nullptr;

        audio_mixer_done();
    }
    CoreState(const CoreState&) = delete;
    CoreState& operator=(const CoreState&) = delete;
    CoreState(CoreState&&) = delete;
    CoreState& operator=(CoreState&&) = delete;

    const bool initialized = true;

    void Run();
private:
    audio_mixer_sound_t* _freezerSound = nullptr;
    audio_mixer_voice_t* _freezerVoice = nullptr;
};

namespace
{
    retro_video_refresh_t _video_refresh = nullptr;
    retro_audio_sample_t _audio_sample = nullptr;
    retro_audio_sample_batch_t _audio_sample_batch = nullptr;
    retro_input_poll_t _input_poll = nullptr;
    retro_input_state_t _input_state = nullptr;
    retro_environment_t _environment = nullptr;
    retro_log_printf_t _log = nullptr;
    alignas(CoreState) std::array<std::byte, sizeof(CoreState)> CoreStateBuffer;
    CoreState& Core = *reinterpret_cast<CoreState*>(CoreStateBuffer.data());

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

RETRO_API void retro_set_audio_sample(retro_audio_sample_t audio_sample)
{
    _audio_sample = audio_sample;
}

RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t audio_sample_batch)
{
    _audio_sample_batch = audio_sample_batch;
}

RETRO_API void retro_set_input_poll(retro_input_poll_t input_poll)
{
    _input_poll = input_poll;
}

RETRO_API void retro_set_input_state(retro_input_state_t input_state)
{
    _input_state = input_state;
}

RETRO_API void retro_set_environment(retro_environment_t env)
{
    _environment = env;
    retro_log_callback log = { .log = nullptr };
    bool yes = true;
    retro_pixel_format format = RETRO_PIXEL_FORMAT_XRGB8888;
    _environment(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &yes);
    _environment(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log);
    _environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format);

    if (!_log && log.log)
    {
        _log = log.log;
        _log(RETRO_LOG_DEBUG, "Loggin' in the air\n");
    }
}


RETRO_API void retro_init()
{
    CoreStateBuffer.fill({});
    new(&CoreStateBuffer) CoreState(); // placement-new the CoreState
    retro_assert(Core.initialized);
}


RETRO_API void retro_deinit()
{
    Core.~CoreState(); // placement delete
    CoreStateBuffer.fill({});
    retro_assert(!Core.initialized);
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
    info->geometry.base_width = SCREEN_WIDTH;
    info->geometry.base_height = SCREEN_HEIGHT;
    info->geometry.max_width = SCREEN_WIDTH;
    info->geometry.max_height = SCREEN_HEIGHT;
    info->timing.fps = 60.0;
    info->timing.sample_rate = SAMPLE_RATE;
}

RETRO_API void retro_set_controller_port_device(unsigned, unsigned) {}

RETRO_API void retro_reset(void)
{
    // TODO: Reset the game
    // TODO: Random probability of failure
}

RETRO_API size_t retro_serialize_size(void)
{
    return 0;
}

/* Serializes internal state. If failed, or size is lower than
 * retro_serialize_size(), it should return false, true otherwise. */
RETRO_API bool retro_serialize(void *data, size_t size) { return false; }
RETRO_API bool retro_unserialize(const void *data, size_t size) { return false; }

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

std::array<uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT> framebuffer {};

RETRO_API void retro_run()
{
    Core.Run();
}

void CoreState::Run()
{
    _input_poll();
    array<float, SAMPLE_RATE * 2 / 60> buffer {};
    array<int16_t, SAMPLE_RATE * 2 / 60> outbuffer {};

    audio_mixer_mix(buffer.data(), buffer.size() / 2, 1.0f, false);
    convert_float_to_s16(outbuffer.data(), buffer.data(), buffer.size());

    _video_refresh(framebuffer.data(), SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH * sizeof(uint32_t));
    _audio_sample_batch(outbuffer.data(), outbuffer.size() / 2);
}