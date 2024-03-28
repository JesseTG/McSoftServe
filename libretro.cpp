// MIT-licensed, see LICENSE in the root directory.

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>

#include <libretro.h>
#include <pntr.h>
#include <pntr_nuklear.h>
#include <retro_assert.h>
#include <audio/audio_mixer.h>
#include <audio/conversion/float_to_s16.h>

#include <embedded/mctaylor_freezer.h>
#include <embedded/mctaylor_bg.h>
#include <embedded/mctaylor_bg_matte.h>
#include <embedded/mctaylor_lcd_font.h>
#include <embedded/mctaylor_panel.h>

using std::array;

constexpr int SAMPLE_RATE = 44100;
constexpr int SCREEN_WIDTH = 1366;
constexpr int SCREEN_HEIGHT = 768;
constexpr int MATTE_PANEL_OFFSET = 70;
constexpr nk_panel_flags WINDOW_FLAGS = static_cast<nk_panel_flags>(NK_WINDOW_BACKGROUND | NK_WINDOW_NO_SCROLLBAR);
constexpr struct nk_rect WINDOW_BOUNDS = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};

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

        _font = pntr_load_font_ttf_from_memory(embedded_mctaylor_lcd_font, sizeof(embedded_mctaylor_lcd_font), 32);
        retro_assert(_font != nullptr);

        _nk = pntr_load_nuklear(_font);
        retro_assert(_nk != nullptr);

        // pntr_load_image_from_memory detects the image type from the data,
        // so I'm leaving it as PNTR_IMAGE_TYPE_UNKNOWN
        _steel_bg = pntr_load_image_from_memory(PNTR_IMAGE_TYPE_UNKNOWN, embedded_mctaylor_bg, sizeof(embedded_mctaylor_bg));
        retro_assert(_steel_bg != nullptr);
        _nk_steel_bg = pntr_image_nk(_steel_bg);

        _matte_bg = pntr_load_image_from_memory(PNTR_IMAGE_TYPE_UNKNOWN, embedded_mctaylor_bg_matte, sizeof(embedded_mctaylor_bg_matte));
        retro_assert(_matte_bg != nullptr);
        _nk_matte_bg = pntr_image_nk(_matte_bg);

        _panel = pntr_load_image_from_memory(PNTR_IMAGE_TYPE_UNKNOWN, embedded_mctaylor_panel, sizeof(embedded_mctaylor_panel));
        retro_assert(_panel != nullptr);
        _nk_panel = pntr_image_nk(_panel);

        _framebuffer = pntr_new_image(SCREEN_WIDTH, SCREEN_HEIGHT);
        retro_assert(_framebuffer != nullptr);

        _nk->style.window.fixed_background.type = NK_STYLE_ITEM_IMAGE;
        _nk->style.window.fixed_background = {
            .type = NK_STYLE_ITEM_IMAGE,
            .data = { .image = _nk_steel_bg }
        };
        _nk->style.window.header.padding = {0, 0};
        _nk->style.window.header.spacing = {0, 0};
        _nk->style.window.padding = {0, 0};
        _nk->style.window.spacing = {0, 0};
        _nk->style.window.group_padding = {0, 0};
        _nk->style.window.border = 0;
    }

    ~CoreState() noexcept
    {
        pntr_unload_image(_framebuffer);
        _framebuffer = nullptr;

        pntr_unload_image(_matte_bg);
        _matte_bg = nullptr;
        _nk_matte_bg.handle.ptr = nullptr;

        pntr_unload_image(_panel);
        _panel = nullptr;
        _nk_panel.handle.ptr = nullptr;

        pntr_unload_image(_steel_bg);
        _steel_bg = nullptr;
        _nk_steel_bg.handle.ptr = nullptr;

        pntr_unload_nuklear(_nk);
        _nk = nullptr;

        pntr_unload_font(_font);
        _font = nullptr;

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
    pntr_font* _font = nullptr;
    nk_context* _nk = nullptr;

    struct nk_image _nk_steel_bg {};
    struct nk_image _nk_matte_bg {};
    struct nk_image _nk_panel {};
    pntr_image* _steel_bg = nullptr;
    pntr_image* _matte_bg = nullptr;
    pntr_image* _panel = nullptr;
    pntr_image* _framebuffer = nullptr;
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

    if (nk_begin(_nk, "", WINDOW_BOUNDS, WINDOW_FLAGS)) {

        nk_layout_row_static(_nk, MATTE_PANEL_OFFSET, SCREEN_WIDTH, 1);
        nk_layout_row_static(_nk, SCREEN_HEIGHT - MATTE_PANEL_OFFSET, SCREEN_WIDTH, 1);
        nk_image(_nk, _nk_matte_bg);
        if (nk_button_label(_nk, "Button")) {
            _log(RETRO_LOG_INFO, "Hello World!\n");
        }
    }
    nk_end(_nk);

    pntr_draw_nuklear(_framebuffer, _nk);
    _video_refresh(_framebuffer->data, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH * sizeof(pntr_color));
    _audio_sample_batch(outbuffer.data(), outbuffer.size() / 2);
}