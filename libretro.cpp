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

#include <embedded/mcsoftserve_freezer.h>
#include <embedded/mcsoftserve_bg.h>
#include <embedded/mcsoftserve_lcd_font.h>
#include <embedded/mcsoftserve_auto_button.h>
#include <embedded/mcsoftserve_wash_button.h>
#include <embedded/mcsoftserve_standby_button.h>
#include <embedded/mcsoftserve_topping_button.h>
#include <embedded/mcsoftserve_sel_button.h>
#include <embedded/mcsoftserve_up_button.h>

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
    CoreState() noexcept :
        _auto_button(embedded_mcsoftserve_auto_button, sizeof(embedded_mcsoftserve_auto_button), PNTR_SKYBLUE, false, false),
        _wash_button(embedded_mcsoftserve_wash_button, sizeof(embedded_mcsoftserve_wash_button), PNTR_SKYBLUE, false, false),
        _standby_button(embedded_mcsoftserve_standby_button, sizeof(embedded_mcsoftserve_standby_button), PNTR_SKYBLUE, false, false),
        _topping_button_l(embedded_mcsoftserve_topping_button, sizeof(embedded_mcsoftserve_topping_button), PNTR_SKYBLUE, false, false),
        _topping_button_r(embedded_mcsoftserve_topping_button, sizeof(embedded_mcsoftserve_topping_button), PNTR_SKYBLUE, true, false),
        _sel_button(embedded_mcsoftserve_sel_button, sizeof(embedded_mcsoftserve_sel_button), PNTR_SKYBLUE, false, false),
        _up_button(embedded_mcsoftserve_up_button, sizeof(embedded_mcsoftserve_up_button), PNTR_SKYBLUE, false, false),
        _down_button(embedded_mcsoftserve_up_button, sizeof(embedded_mcsoftserve_up_button), PNTR_SKYBLUE, false, true)
    {
        audio_mixer_init(SAMPLE_RATE);
        _freezerSound = audio_mixer_load_wav(
            (void*)embedded_mcsoftserve_freezer,
            sizeof(embedded_mcsoftserve_freezer),
            "sinc",
            RESAMPLER_QUALITY_HIGHEST
        );
        retro_assert(_freezerSound != nullptr);

        _freezerVoice = audio_mixer_play(_freezerSound, true, 1.0f, "sinc", RESAMPLER_QUALITY_HIGHEST, nullptr);
        retro_assert(_freezerVoice != nullptr);

        _font = pntr_load_font_ttf_from_memory(embedded_mcsoftserve_lcd_font, sizeof(embedded_mcsoftserve_lcd_font), 32);
        retro_assert(_font != nullptr);

        _nk = pntr_load_nuklear(_font);
        retro_assert(_nk != nullptr);

        // pntr_load_image_from_memory detects the image type from the data,
        // so I'm leaving it as PNTR_IMAGE_TYPE_UNKNOWN
        _steel_bg = pntr_load_image_from_memory(PNTR_IMAGE_TYPE_UNKNOWN, embedded_mcsoftserve_bg, sizeof(embedded_mcsoftserve_bg));
        retro_assert(_steel_bg != nullptr);
        _nk_steel_bg = pntr_image_nk(_steel_bg);
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
    struct button
    {
        pntr_image* normal_button = nullptr;
        struct nk_image nk_normal_button {};
        pntr_image* active_button = nullptr;
        struct nk_image nk_active_button {};
        nk_style_button style {};

        button(const uint8_t* data, size_t length, pntr_color active_tint, bool flipx, bool flipy)
        {
            retro_assert(data != nullptr);
            normal_button = pntr_load_image_from_memory(PNTR_IMAGE_TYPE_UNKNOWN, data, length);
            pntr_image_flip(normal_button, flipx, flipy);

            active_button = pntr_image_copy(normal_button);
            retro_assert(active_button != nullptr);
            pntr_image_color_tint(active_button, active_tint);

            style.normal.type = NK_STYLE_ITEM_IMAGE;
            style.normal.data.image = pntr_image_nk(normal_button);
            style.active.type = NK_STYLE_ITEM_IMAGE;
            style.active.data.image = pntr_image_nk(active_button);
        }

        button(const button&) = delete;
        button& operator=(const button&) = delete;
        button(button&&) = delete;
        ~button() noexcept
        {
            pntr_unload_image(normal_button);
            normal_button = nullptr;
            nk_normal_button.handle.ptr = nullptr;

            pntr_unload_image(active_button);
            active_button = nullptr;
            nk_active_button.handle.ptr = nullptr;
        }
    };

    audio_mixer_sound_t* _freezerSound = nullptr;
    audio_mixer_voice_t* _freezerVoice = nullptr;
    pntr_font* _font = nullptr;
    nk_context* _nk = nullptr;

    struct nk_image _nk_steel_bg {};
    pntr_image* _steel_bg = nullptr;
    button _auto_button;
    button _wash_button;
    button _standby_button;
    button _topping_button_l;
    button _topping_button_r;
    button _sel_button;
    button _up_button;
    button _down_button;
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
    info->library_name = "McSoftServe";
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

// Thanks, Rob!
float pntr_app_libretro_mouse_pointer_convert(float coord, float full, float margin)
{
    float max         = (float)0x7fff;
    float screenCoord = (((coord + max) / (max * 2.0f) ) * full) - margin;

    // Keep the mouse on the screen.
    if (margin > 0.0f) {
        float limit = full - (margin * 2.0f) - 1.0f;
        screenCoord = (screenCoord < 0.0f)  ? 0.0f  : screenCoord;
        screenCoord = (screenCoord > limit) ? limit : screenCoord;
    }

    return screenCoord + 0.5f;
}


void CoreState::Run()
{
    _input_poll();

    nk_input_begin(_nk);
    {
        int16_t mouseX = _input_state(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
        int16_t mouseY = _input_state(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);
        int16_t pressed = _input_state(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED);

        int screenX = pntr_app_libretro_mouse_pointer_convert(mouseX, SCREEN_WIDTH, 0);
        int screenY = pntr_app_libretro_mouse_pointer_convert(mouseY, SCREEN_HEIGHT, 0);
        nk_input_motion(_nk, screenX, screenY);
        nk_input_button(_nk, NK_BUTTON_LEFT, screenX, screenY, pressed);
    }
    nk_input_end(_nk);

    array<float, SAMPLE_RATE * 2 / 60> buffer {};
    array<int16_t, SAMPLE_RATE * 2 / 60> outbuffer {};

    audio_mixer_mix(buffer.data(), buffer.size() / 2, 1.0f, false);
    convert_float_to_s16(outbuffer.data(), buffer.data(), buffer.size());

    if (nk_begin(_nk, "", WINDOW_BOUNDS, WINDOW_FLAGS)) {
        nk_layout_space_begin(_nk, NK_STATIC, 500, INT_MAX);
        nk_layout_space_push(_nk, nk_rect(0,0,150,200));
        if (nk_button_image_styled(_nk, nullptr, {}))
        {

        }
        if (nk_button_color(_nk, nk_color{ 255, 0, 0, 255 })) {
            _log(RETRO_LOG_DEBUG, "Button pressed\n");
        }
        nk_layout_space_end(_nk);
    }
    nk_end(_nk);

    pntr_draw_nuklear(_framebuffer, _nk);
    _video_refresh(_framebuffer->data, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH * sizeof(pntr_color));
    _audio_sample_batch(outbuffer.data(), outbuffer.size() / 2);
}