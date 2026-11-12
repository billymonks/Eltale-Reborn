#include <algorithm>

#include "recomp_ui.h"
#include "recomp_input.h"
#include "zelda_sound.h"
#include "zelda_config.h"
#include "zelda_debug.h"
#include "zelda_render.h"
#include "zelda_support.h"
#include "promptfont.h"
#include "ultramodern/config.hpp"
#include "ultramodern/ultramodern.hpp"
#include "RmlUi/Core.h"

#include <functional>

#include "core/ui_context.h"

ultramodern::renderer::GraphicsConfig new_options;
Rml::DataModelHandle nav_help_model_handle;
Rml::DataModelHandle general_model_handle;
Rml::DataModelHandle controls_model_handle;
Rml::DataModelHandle graphics_model_handle;
Rml::DataModelHandle sound_options_model_handle;

// True if controller config menu is open, false if keyboard config menu is open, undefined otherwise
bool configuring_controller = false;

int recompui::config_tab_to_index(recompui::ConfigTab tab) {
    switch (tab) {
    case recompui::ConfigTab::General:
        return 0;
    case recompui::ConfigTab::Controls:
        return 1;
    case recompui::ConfigTab::Graphics:
        return 2;
    case recompui::ConfigTab::Sound:
        return 3;
    case recompui::ConfigTab::Mods:
        return 4;
    case recompui::ConfigTab::Debug:
        return 5;
    default:
        assert(false && "Unknown config tab.");
        return 0;
    }
}

template <typename T>
void get_option(const T& input, Rml::Variant& output) {
    std::string value = "";
    to_json(value, input);

    if (value.empty()) {
        throw std::runtime_error("Invalid value :" + std::to_string(int(input)));
    }

    output = value;
}

template <typename T>
void set_option(T& output, const Rml::Variant& input) {
    T value = T::OptionCount;
    from_json(input.Get<std::string>(), value);

    if (value == T::OptionCount) {
        throw std::runtime_error("Invalid value :" + input.Get<std::string>());
    }

    output = value;
}

template <typename T>
void bind_option(Rml::DataModelConstructor& constructor, const std::string& name, T* option) {
    constructor.BindFunc(name,
        [option](Rml::Variant& out) { get_option(*option, out); },
        [option](const Rml::Variant& in) {
            set_option(*option, in);
            graphics_model_handle.DirtyVariable("options_changed");
            graphics_model_handle.DirtyVariable("ds_info");
        }
    );
};

template <typename T>
void bind_atomic(Rml::DataModelConstructor& constructor, Rml::DataModelHandle handle, const char* name, std::atomic<T>* atomic_val) {
    constructor.BindFunc(name,
        [atomic_val](Rml::Variant& out) {
            out = atomic_val->load();
        },
        [atomic_val, handle, name](const Rml::Variant& in) mutable {
            atomic_val->store(in.Get<T>());
            handle.DirtyVariable(name);
        }
    );
}

static int scanned_binding_index = -1;
static int scanned_input_index = -1;
static int focused_input_index = -1;
static int focused_config_option_index = -1;

static bool msaa2x_supported = false;
static bool msaa4x_supported = false;
static bool msaa8x_supported = false;
static bool sample_positions_supported = false;

static bool cont_active = true;

static recomp::InputDevice cur_device = recomp::InputDevice::Controller;

int recomp::get_scanned_input_index() {
    return scanned_input_index;
}

void recomp::finish_scanning_input(recomp::InputField scanned_field) {
    recomp::set_input_binding(static_cast<recomp::GameInput>(scanned_input_index), scanned_binding_index, cur_device, scanned_field);
    scanned_input_index = -1;
    scanned_binding_index = -1;
    controls_model_handle.DirtyVariable("inputs");
    controls_model_handle.DirtyVariable("active_binding_input");
    controls_model_handle.DirtyVariable("active_binding_slot");
    nav_help_model_handle.DirtyVariable("nav_help__accept");
    nav_help_model_handle.DirtyVariable("nav_help__exit");
    graphics_model_handle.DirtyVariable("gfx_help__apply");
}

void recomp::cancel_scanning_input() {
    recomp::stop_scanning_input();
    scanned_input_index = -1;
    scanned_binding_index = -1;
    controls_model_handle.DirtyVariable("inputs");
    controls_model_handle.DirtyVariable("active_binding_input");
    controls_model_handle.DirtyVariable("active_binding_slot");
    nav_help_model_handle.DirtyVariable("nav_help__accept");
    nav_help_model_handle.DirtyVariable("nav_help__exit");
    graphics_model_handle.DirtyVariable("gfx_help__apply");
}

void recomp::config_menu_set_cont_or_kb(bool cont_interacted) {
    if (cont_active != cont_interacted) {
        cont_active = cont_interacted;

        if (nav_help_model_handle) {
            nav_help_model_handle.DirtyVariable("nav_help__navigate");
            nav_help_model_handle.DirtyVariable("nav_help__accept");
            nav_help_model_handle.DirtyVariable("nav_help__exit");
        }

        if (graphics_model_handle) {
            graphics_model_handle.DirtyVariable("gfx_help__apply");
        }
    }
}

void close_config_menu_impl() {
    zelda64::save_config();

    recompui::ContextId config_context = recompui::get_config_context_id();
    recompui::ContextId sub_menu_context = recompui::get_config_sub_menu_context_id();

    if (recompui::is_context_shown(sub_menu_context)) {
    	recompui::hide_context(sub_menu_context);
    }
    else {
    	recompui::hide_context(config_context);
    }

    if (!ultramodern::is_game_started()) {
        recompui::show_context(recompui::get_launcher_context_id(), "");
    }
}

// TODO: Remove once RT64 gets native fullscreen support on Linux
#if defined(__linux__)
extern SDL_Window* window;
#endif

void apply_graphics_config(void) {
    ultramodern::renderer::set_graphics_config(new_options);
#if defined(__linux__) // TODO: Remove once RT64 gets native fullscreen support on Linux
    if (new_options.wm_option == ultramodern::renderer::WindowMode::Fullscreen) {
        SDL_SetWindowFullscreen(window,SDL_WINDOW_FULLSCREEN_DESKTOP);
    } else {
        SDL_SetWindowFullscreen(window,0);
    }
#endif
}

void close_config_menu() {
    if (ultramodern::renderer::get_graphics_config() != new_options) {
        recompui::open_choice_prompt(
            "Graphics options have changed",
            "Would you like to apply or discard the changes?",
            "Apply",
            "Discard",
            []() {
                apply_graphics_config();
                graphics_model_handle.DirtyAllVariables();
                close_config_menu_impl();
            },
            []() {
                new_options = ultramodern::renderer::get_graphics_config();
                graphics_model_handle.DirtyAllVariables();
                close_config_menu_impl();
            },
            recompui::ButtonVariant::Success,
            recompui::ButtonVariant::Error,
            true,
            "config__close-menu-button"
        );
        return;
    }

    close_config_menu_impl();
}

void zelda64::open_quit_game_prompt() {
    recompui::open_choice_prompt(
        "Are you sure you want to quit?",
        "Any progress since your last save will be lost.",
        "Quit",
        "Cancel",
        []() {
            ultramodern::quit();
        },
        []() {},
        recompui::ButtonVariant::Error,
        recompui::ButtonVariant::Tertiary,
        true,
        "config__quit-game-button"
    );
}

// These defaults values don't matter, as the config file handling overrides them.
struct ControlOptionsContext {
    int rumble_strength; // 0 to 100
    int gyro_sensitivity; // 0 to 100
    int mouse_sensitivity; // 0 to 100
    int joystick_deadzone; // 0 to 100
    zelda64::TargetingMode targeting_mode;
    recomp::BackgroundInputMode background_input_mode;
    zelda64::FilmGrainMode film_grain_mode;
    zelda64::RtDebugKeysMode rt_debug_keys_mode;
    zelda64::RadioBoxMode radio_comm_box_mode;
    zelda64::AimInvertMode invert_y_axis_mode;
    zelda64::AimInvertMode analog_camera_invert_mode;
    int camera_turn_speed; // percent, 100 = the tuned speed
    int camera_tilt_speed;
    zelda64::AimInvertMode camera_invert_turn_mode;
    zelda64::AimInvertMode camera_invert_tilt_mode;
};

ControlOptionsContext control_options_context;

int recomp::get_rumble_strength() {
    return control_options_context.rumble_strength;
}

void recomp::set_rumble_strength(int strength) {
    control_options_context.rumble_strength = strength;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("rumble_strength");
    }
}

int recomp::get_gyro_sensitivity() {
    return control_options_context.gyro_sensitivity;
}

int recomp::get_mouse_sensitivity() {
    return control_options_context.mouse_sensitivity;
}

int recomp::get_joystick_deadzone() {
    return control_options_context.joystick_deadzone;
}

void recomp::set_gyro_sensitivity(int sensitivity) {
    control_options_context.gyro_sensitivity = sensitivity;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("gyro_sensitivity");
    }
}

void recomp::set_mouse_sensitivity(int sensitivity) {
    control_options_context.mouse_sensitivity = sensitivity;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("mouse_sensitivity");
    }
}

void recomp::set_joystick_deadzone(int deadzone) {
    control_options_context.joystick_deadzone = deadzone;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("joystick_deadzone");
    }
}

zelda64::TargetingMode zelda64::get_targeting_mode() {
    return control_options_context.targeting_mode;
}

void zelda64::set_targeting_mode(zelda64::TargetingMode mode) {
    control_options_context.targeting_mode = mode;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("targeting_mode");
    }
}

recomp::BackgroundInputMode recomp::get_background_input_mode() {
    return control_options_context.background_input_mode;
}

void recomp::set_background_input_mode(recomp::BackgroundInputMode mode) {
    control_options_context.background_input_mode = mode;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("background_input_mode");
    }
    SDL_SetHint(
        SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,
        mode == recomp::BackgroundInputMode::On
            ? "1"
            : "0"
    );
}

zelda64::FilmGrainMode zelda64::get_film_grain_mode() {
    return control_options_context.film_grain_mode;
}

void zelda64::set_film_grain_mode(zelda64::FilmGrainMode mode) {
    control_options_context.film_grain_mode = mode;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("film_grain_mode");
    }
}

zelda64::RtDebugKeysMode zelda64::get_rt_debug_keys_mode() {
    return control_options_context.rt_debug_keys_mode;
}

void zelda64::set_rt_debug_keys_mode(zelda64::RtDebugKeysMode mode) {
    control_options_context.rt_debug_keys_mode = mode;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("rt_debug_keys_mode");
    }
}

zelda64::RadioBoxMode zelda64::get_radio_comm_box_mode() {
    return control_options_context.radio_comm_box_mode;
}

void zelda64::set_radio_comm_box_mode(zelda64::RadioBoxMode mode) {
    control_options_context.radio_comm_box_mode = mode;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("radio_comm_box_mode");
    }
}

zelda64::AimInvertMode zelda64::get_invert_y_axis_mode() {
    return control_options_context.invert_y_axis_mode;
}

void zelda64::set_invert_y_axis_mode(zelda64::AimInvertMode mode) {
    control_options_context.invert_y_axis_mode = mode;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("invert_y_axis_mode");
    }
}

zelda64::AimInvertMode zelda64::get_analog_camera_invert_mode() {
    return control_options_context.analog_camera_invert_mode;
}

void zelda64::set_analog_camera_invert_mode(zelda64::AimInvertMode mode) {
    control_options_context.analog_camera_invert_mode = mode;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("analog_camera_invert_mode");
    }
}

int recomp::get_camera_turn_speed() {
    return control_options_context.camera_turn_speed;
}

int recomp::get_camera_tilt_speed() {
    return control_options_context.camera_tilt_speed;
}

void recomp::set_camera_turn_speed(int speed) {
    control_options_context.camera_turn_speed = speed;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("camera_turn_speed");
    }
}

void recomp::set_camera_tilt_speed(int speed) {
    control_options_context.camera_tilt_speed = speed;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("camera_tilt_speed");
    }
}

zelda64::AimInvertMode zelda64::get_camera_invert_turn_mode() {
    return control_options_context.camera_invert_turn_mode;
}

void zelda64::set_camera_invert_turn_mode(zelda64::AimInvertMode mode) {
    control_options_context.camera_invert_turn_mode = mode;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("camera_invert_turn_mode");
    }
}

zelda64::AimInvertMode zelda64::get_camera_invert_tilt_mode() {
    return control_options_context.camera_invert_tilt_mode;
}

void zelda64::set_camera_invert_tilt_mode(zelda64::AimInvertMode mode) {
    control_options_context.camera_invert_tilt_mode = mode;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("camera_invert_tilt_mode");
    }
}

struct SoundOptionsContext {
    std::atomic<int> main_volume; // Option to control the volume of all sound
    std::atomic<int> bgm_volume;
    std::atomic<int> sfx_volume;
	std::atomic<int> voice_volume;
    std::atomic<int> low_health_beeps_enabled; // RmlUi doesn't seem to like "true"/"false" strings for setting variants so an int is used here instead.
    void reset() {
        bgm_volume = 100;
        sfx_volume = 100;
		voice_volume = 100;
        main_volume = 100;
        low_health_beeps_enabled = (int)true;
    }
    SoundOptionsContext() {
        reset();
    }
};

SoundOptionsContext sound_options_context;

void zelda64::reset_sound_settings() {
    sound_options_context.reset();
    if (sound_options_model_handle) {
        sound_options_model_handle.DirtyAllVariables();
    }
}

void zelda64::set_main_volume(int volume) {
    sound_options_context.main_volume.store(volume);
    if (sound_options_model_handle) {
        sound_options_model_handle.DirtyVariable("main_volume");
    }
}

int zelda64::get_main_volume() {
    return sound_options_context.main_volume.load();
}

void zelda64::set_bgm_volume(int volume) {
    sound_options_context.bgm_volume.store(volume);
    if (sound_options_model_handle) {
        sound_options_model_handle.DirtyVariable("bgm_volume");
    }
}

int zelda64::get_bgm_volume() {
    return sound_options_context.bgm_volume.load();
}

void zelda64::set_sfx_volume(int volume) {
    sound_options_context.sfx_volume.store(volume);
	if (sound_options_model_handle) {
		sound_options_model_handle.DirtyVariable("sfx_volume");
	}
}

int zelda64::get_sfx_volume() {
    return sound_options_context.sfx_volume.load();
}

void zelda64::set_voice_volume(int volume) {
    sound_options_context.voice_volume.store(volume);
	if (sound_options_model_handle) {
		sound_options_model_handle.DirtyVariable("voice_volume");
	}
}

int zelda64::get_voice_volume() {
    return sound_options_context.voice_volume.load();
}

void zelda64::set_low_health_beeps_enabled(bool enabled) {
    sound_options_context.low_health_beeps_enabled.store((int)enabled);
    if (sound_options_model_handle) {
        sound_options_model_handle.DirtyVariable("low_health_beeps_enabled");
    }
}

bool zelda64::get_low_health_beeps_enabled() {
    return (bool)sound_options_context.low_health_beeps_enabled.load();
}

struct DebugContext {
    Rml::DataModelHandle model_handle;
    std::vector<std::string> area_names;
    std::vector<std::string> scene_names;
    std::vector<std::string> entrance_names; 
    int area_index = 0;
    int scene_index = 0;
    int entrance_index = 0;
    int set_time_day = 1;
    int set_time_hour = 12;
    int set_time_minute = 0;
    bool debug_enabled = false;

    DebugContext() {
        for (const auto& area : zelda64::game_warps) {
            area_names.emplace_back(area.name);
        }
        update_warp_names();
    }

    void update_warp_names() {
        scene_names.clear();
        for (const auto& scene : zelda64::game_warps[area_index].scenes) {
            scene_names.emplace_back(scene.name);
        }
        
        entrance_names = zelda64::game_warps[area_index].scenes[scene_index].entrances;
    }
};

DebugContext debug_context;

// ---- The Controls page's picture: a modern pad, each control labelled with what it does ----
// A reverse lookup of the controller's bindings: for each physical control (by SDL's positions: A south, B east, X
// west, Y north), the actions bound to it, laid out like Steam's controller pages: the shoulder and menu buttons beside
// a drawing of the pad, the sticks, D-pad and face buttons in sections under it. It hangs off the page's "inputs"
// variable (inputs.pad), which the page refreshes after every change of a binding and of the highlighted row.
struct PadLine {
    Rml::String text;                       // (a struct, not a bare string: the debug page registers vector<string>)
};

struct PadRow {
    Rml::String glyph;                      // the control's prompt-font glyph, or a short name
    bool glyph_is_text = false;             // the glyph is a name (a paddle, Share), not a prompt-font glyph
    Rml::Vector<PadLine> lines;             // the actions bound to it, one a line; empty: nothing
    bool lit = false;                       // bound to the highlighted row
};

// The controls on the drawing, lit when bound to the highlighted row.
struct PadSpots {
    bool lt = false, lb = false, rt = false, rb = false, view = false, menu = false;
    bool ls = false, l3 = false, rs = false, r3 = false;
    bool dpad_up = false, dpad_down = false, dpad_left = false, dpad_right = false;
    bool a = false, b = false, x = false, y = false;
};

struct PadView {
    Rml::Vector<PadRow> left, right;                            // beside the drawing
    Rml::Vector<PadRow> left_stick, dpad, right_stick, face;    // the sections under it
    PadSpots spots;
};

static PadView pad_view;
static Rml::VariableDefinition* pad_view_definition = nullptr;
static size_t pad_view_signature = 0;

// Whether an input belongs on the picture: the current controls style's (Classic: N64 L and Z, no camera actions;
// Modern: the reverse), but not the PC menu's own Accept and Apply, which only work in that menu.
static bool pad_lists_input(recomp::GameInput input) {
    if (input == recomp::GameInput::ACCEPT_MENU || input == recomp::GameInput::APPLY_MENU) {
        return false;
    }
    return recomp::is_input_used_by_style(input, recomp::get_controls_style());
}

// The listed inputs bound to one control on the controller (either binding slot), in the controls page's order.
static Rml::Vector<recomp::GameInput> pad_inputs_on(const recomp::InputField& field) {
    Rml::Vector<recomp::GameInput> out;
    for (size_t i = 0; i < recomp::get_num_inputs(); i++) {
        recomp::GameInput input = static_cast<recomp::GameInput>(i);
        if (!pad_lists_input(input)) {
            continue;
        }
        for (size_t slot = 0; slot < recomp::bindings_per_input; slot++) {
            if (recomp::get_input_binding(input, slot, recomp::InputDevice::Controller) == field) {
                out.push_back(input);
                break;
            }
        }
    }
    return out;
}

static bool pad_has_focused(const Rml::Vector<recomp::GameInput>& inputs) {
    for (recomp::GameInput input : inputs) {
        if (static_cast<int>(input) == focused_input_index) {
            return true;
        }
    }
    return false;
}

static PadRow pad_row(const recomp::InputField& field, const char* name = nullptr) {
    PadRow row;
    row.glyph = name ? Rml::String{name} : field.to_string();
    row.glyph_is_text = name != nullptr;
    Rml::Vector<recomp::GameInput> inputs = pad_inputs_on(field);
    for (recomp::GameInput input : inputs) {
        row.lines.push_back(PadLine{recomp::get_input_name(input)});
    }
    row.lit = pad_has_focused(inputs);
    return row;
}

// An extra button (paddles, Share, the touchpad, Home): listed only when something is bound to it.
static void pad_extra(Rml::Vector<PadRow>& out, int sdl_button, const char* name) {
    PadRow row = pad_row(recomp::controller_button_field(sdl_button), name);
    if (!row.lines.empty()) {
        out.push_back(std::move(row));
    }
}

// A stick's section: one row when its four directions hold exactly one action's four (Move, Camera), else a row a
// direction; then its click. Up is SDL's negative Y.
static void pad_stick(Rml::Vector<PadRow>& out, int axis_x, int axis_y, int click_button, const char* stick_glyph,
                      bool& spot, bool& click_spot) {
    recomp::InputField up = recomp::controller_axis_field(axis_y, false);
    recomp::InputField down = recomp::controller_axis_field(axis_y, true);
    recomp::InputField left = recomp::controller_axis_field(axis_x, false);
    recomp::InputField right = recomp::controller_axis_field(axis_x, true);
    auto only = [](const recomp::InputField& field, recomp::GameInput input) {
        Rml::Vector<recomp::GameInput> inputs = pad_inputs_on(field);
        return inputs.size() == 1 && inputs[0] == input;
    };
    using GI = recomp::GameInput;
    const char* whole = nullptr;
    bool unbound = pad_inputs_on(up).empty() && pad_inputs_on(down).empty() && pad_inputs_on(left).empty() &&
                   pad_inputs_on(right).empty();
    if (unbound) {
        // Nothing on the stick (Classic controls' right stick): one row, "--".
        PadRow row;
        row.glyph = stick_glyph;
        out.push_back(std::move(row));
        spot = false;
    } else if (only(up, GI::Y_AXIS_POS) && only(down, GI::Y_AXIS_NEG) && only(left, GI::X_AXIS_NEG) && only(right, GI::X_AXIS_POS)) {
        whole = "Move";
    } else if (only(up, GI::CAMERA_UP) && only(down, GI::CAMERA_DOWN) && only(left, GI::CAMERA_LEFT) && only(right, GI::CAMERA_RIGHT)) {
        whole = "Camera";
    }
    if (whole != nullptr) {
        PadRow row;
        row.glyph = stick_glyph;
        row.lines.push_back(PadLine{whole});
        for (const recomp::InputField& field : {up, down, left, right}) {
            row.lit = row.lit || pad_has_focused(pad_inputs_on(field));
        }
        spot = row.lit;
        out.push_back(std::move(row));
    } else if (!unbound) {
        for (const recomp::InputField& field : {up, down, left, right}) {
            out.push_back(pad_row(field));
            spot = spot || out.back().lit;
        }
    }
    PadRow click = pad_row(recomp::controller_button_field(click_button));
    click_spot = click.lit;
    if (!unbound || !click.lines.empty()) {     // a stick with nothing on it at all is the one "--" row
        out.push_back(std::move(click));
    }
}

static void pad_build() {
    PadView view;
    PadSpots& s = view.spots;
    auto add = [](Rml::Vector<PadRow>& rows, PadRow row, bool& spot) {
        spot = row.lit;
        rows.push_back(std::move(row));
    };
    add(view.left, pad_row(recomp::controller_axis_field(SDL_CONTROLLER_AXIS_TRIGGERLEFT, true)), s.lt);
    add(view.left, pad_row(recomp::controller_button_field(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)), s.lb);
    add(view.left, pad_row(recomp::controller_button_field(SDL_CONTROLLER_BUTTON_BACK)), s.view);
    pad_extra(view.left, SDL_CONTROLLER_BUTTON_PADDLE2, "L4");
    pad_extra(view.left, SDL_CONTROLLER_BUTTON_PADDLE4, "L5");
    pad_extra(view.left, SDL_CONTROLLER_BUTTON_MISC1, "Share");
    add(view.right, pad_row(recomp::controller_axis_field(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, true)), s.rt);
    add(view.right, pad_row(recomp::controller_button_field(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)), s.rb);
    add(view.right, pad_row(recomp::controller_button_field(SDL_CONTROLLER_BUTTON_START)), s.menu);
    pad_extra(view.right, SDL_CONTROLLER_BUTTON_PADDLE1, "R4");
    pad_extra(view.right, SDL_CONTROLLER_BUTTON_PADDLE3, "R5");
    pad_extra(view.right, SDL_CONTROLLER_BUTTON_TOUCHPAD, nullptr);
    pad_extra(view.right, SDL_CONTROLLER_BUTTON_GUIDE, nullptr);
    pad_stick(view.left_stick, SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY, SDL_CONTROLLER_BUTTON_LEFTSTICK,
              PF_ANALOG_L, s.ls, s.l3);
    // Up, left, down, right: the spells' order in the list (Fire, Earth, Water, Wind).
    add(view.dpad, pad_row(recomp::controller_button_field(SDL_CONTROLLER_BUTTON_DPAD_UP)), s.dpad_up);
    add(view.dpad, pad_row(recomp::controller_button_field(SDL_CONTROLLER_BUTTON_DPAD_LEFT)), s.dpad_left);
    add(view.dpad, pad_row(recomp::controller_button_field(SDL_CONTROLLER_BUTTON_DPAD_DOWN)), s.dpad_down);
    add(view.dpad, pad_row(recomp::controller_button_field(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)), s.dpad_right);
    pad_stick(view.right_stick, SDL_CONTROLLER_AXIS_RIGHTX, SDL_CONTROLLER_AXIS_RIGHTY, SDL_CONTROLLER_BUTTON_RIGHTSTICK,
              PF_ANALOG_R, s.rs, s.r3);
    add(view.face, pad_row(recomp::controller_button_field(SDL_CONTROLLER_BUTTON_A)), s.a);
    add(view.face, pad_row(recomp::controller_button_field(SDL_CONTROLLER_BUTTON_B)), s.b);
    add(view.face, pad_row(recomp::controller_button_field(SDL_CONTROLLER_BUTTON_X)), s.x);
    add(view.face, pad_row(recomp::controller_button_field(SDL_CONTROLLER_BUTTON_Y)), s.y);
    pad_view = std::move(view);
}

// The pad view, rebuilt only when the controller's bindings or the highlighted row changed (RmlUi resolves
// inputs.pad once per view it updates).
static PadView* pad_get() {
    size_t sig = std::hash<int>{}(focused_input_index) * 31 + static_cast<size_t>(recomp::get_controls_style());
    for (size_t i = 0; i < recomp::get_num_inputs(); i++) {
        for (size_t slot = 0; slot < recomp::bindings_per_input; slot++) {
            const recomp::InputField& f = recomp::get_input_binding(static_cast<recomp::GameInput>(i), slot, recomp::InputDevice::Controller);
            sig = sig * 31 + f.input_type;
            sig = sig * 31 + static_cast<uint32_t>(f.input_id);
        }
    }
    if (sig != pad_view_signature || pad_view.face.empty()) {
        pad_view_signature = sig;
        pad_build();
    }
    return &pad_view;
}

recompui::ContextId config_context;

recompui::ContextId recompui::get_config_context_id() {
	return config_context;
}

// Helper copied from RmlUi to get a named child.
Rml::Element* recompui::get_child_by_tag(Rml::Element* parent, const std::string& tag)
{
	// Look for the existing child
	for (int i = 0; i < parent->GetNumChildren(); i++)
	{
        Rml::Element* child = parent->GetChild(i);
		if (child->GetTagName() == tag)
			return child;
	}

    return nullptr;
}

class ConfigTabsetListener : public Rml::EventListener {
    void ProcessEvent(Rml::Event& event) override {
        if (event.GetId() == Rml::EventId::Tabchange) {
            int tab_index = event.GetParameter<int>("tab_index", 0);
            bool in_mod_tab = (tab_index == recompui::config_tab_to_index(recompui::ConfigTab::Mods));
            if (in_mod_tab) {
                recompui::set_config_tabset_mod_nav();
            }
            else {
                Rml::ElementTabSet* tabset = recompui::get_config_tabset();
                Rml::Element* tabs = recompui::get_child_by_tag(tabset, "tabs");
                if (tabs != nullptr) {
                    size_t num_children = tabs->GetNumChildren();
                    for (size_t i = 0; i < num_children; i++) {
                        tabs->GetChild(i)->SetProperty(Rml::PropertyId::NavDown, Rml::Style::Nav::Auto);
                    }
                }
            }
        }
    }
};

class ConfigMenu : public recompui::MenuController {
private:
    ConfigTabsetListener config_tabset_listener;
public:
    ConfigMenu() {

    }
    ~ConfigMenu() override {

    }
    void load_document() override {
		config_context = recompui::create_context(zelda64::get_asset_path("config_menu.rml"));
        recompui::update_mod_list(false);
        recompui::get_config_tabset()->AddEventListener(Rml::EventId::Tabchange, &config_tabset_listener);
    }
    void register_events(recompui::UiEventListenerInstancer& listener) override {
        recompui::register_event(listener, "apply_options",
            [](const std::string& param, Rml::Event& event) {
                graphics_model_handle.DirtyVariable("options_changed");
                apply_graphics_config();
            });
        recompui::register_event(listener, "config_keydown",
            [](const std::string& param, Rml::Event& event) {
                if (!recompui::is_prompt_open() && event.GetId() == Rml::EventId::Keydown) {
                    auto key = event.GetParameter<Rml::Input::KeyIdentifier>("key_identifier", Rml::Input::KeyIdentifier::KI_UNKNOWN);
                    switch (key) {
                        case Rml::Input::KeyIdentifier::KI_ESCAPE:
                            close_config_menu();
                            break;
                        case Rml::Input::KeyIdentifier::KI_F:
                            graphics_model_handle.DirtyVariable("options_changed");
                            apply_graphics_config();
                            break;
                    }
                }
            });
        // This needs to be separate from `close_config_menu` so it ensures that the event is only on the target
        recompui::register_event(listener, "close_config_menu_backdrop",
            [](const std::string& param, Rml::Event& event) {
                if (event.GetPhase() == Rml::EventPhase::Target) {
                    close_config_menu();
                }
            });
        recompui::register_event(listener, "close_config_menu",
            [](const std::string& param, Rml::Event& event) {
                close_config_menu();
            });

        recompui::register_event(listener, "open_quit_game_prompt",
            [](const std::string& param, Rml::Event& event) {
                zelda64::open_quit_game_prompt();
            });

        recompui::register_event(listener, "toggle_input_device",
            [](const std::string& param, Rml::Event& event) {
                cur_device = cur_device == recomp::InputDevice::Controller
                    ? recomp::InputDevice::Keyboard
                    : recomp::InputDevice::Controller;
                controls_model_handle.DirtyVariable("input_device_is_keyboard");
                controls_model_handle.DirtyVariable("inputs");
            });
            
        recompui::register_event(listener, "area_index_changed",
            [](const std::string& param, Rml::Event& event) {
                debug_context.area_index = event.GetParameter<int>("value", 0);
                debug_context.scene_index = 0;
                debug_context.entrance_index = 0;
                debug_context.update_warp_names();
                debug_context.model_handle.DirtyVariable("scene_index");
                debug_context.model_handle.DirtyVariable("entrance_index");
                debug_context.model_handle.DirtyVariable("scene_names");
                debug_context.model_handle.DirtyVariable("entrance_names");
            });
            
        recompui::register_event(listener, "scene_index_changed",
            [](const std::string& param, Rml::Event& event) {
                debug_context.scene_index = event.GetParameter<int>("value", 0);
                debug_context.entrance_index = 0;
                debug_context.update_warp_names();
                debug_context.model_handle.DirtyVariable("entrance_index");
                debug_context.model_handle.DirtyVariable("entrance_names");
            });

        recompui::register_event(listener, "do_warp",
            [](const std::string& param, Rml::Event& event) {
                zelda64::do_warp(debug_context.area_index, debug_context.scene_index, debug_context.entrance_index);
            });

        recompui::register_event(listener, "set_time",
            [](const std::string& param, Rml::Event& event) {
                zelda64::set_time(debug_context.set_time_day, debug_context.set_time_hour, debug_context.set_time_minute);
            });
    }

    void bind_config_list_events(Rml::DataModelConstructor &constructor) {
        constructor.BindEventCallback("set_cur_config_index",
            [](Rml::DataModelHandle model_handle, Rml::Event& event, const Rml::VariantList& inputs) {
                int option_index = inputs.at(0).Get<size_t>();
                // watch for mouseout being overzealous during event bubbling, only clear if the event's attached element matches the current
                if (option_index == -1 && event.GetType() == "mouseout" && event.GetCurrentElement() != event.GetTargetElement()) {
                    return;
                }
                focused_config_option_index = option_index;
                model_handle.DirtyVariable("cur_config_index");
            });

        constructor.Bind("cur_config_index", &focused_config_option_index);
    }

    void make_graphics_bindings(Rml::Context* context) {
        Rml::DataModelConstructor constructor = context->CreateDataModel("graphics_model");
        if (!constructor) {
            throw std::runtime_error("Failed to make RmlUi data model for the graphics config menu");
        }

        ultramodern::sleep_milliseconds(50);
        new_options = ultramodern::renderer::get_graphics_config();
        bind_config_list_events(constructor);

        constructor.BindFunc("res_option",
            [](Rml::Variant& out) { get_option(new_options.res_option, out); },
            [](const Rml::Variant& in) {
                set_option(new_options.res_option, in);
                graphics_model_handle.DirtyVariable("options_changed");
                graphics_model_handle.DirtyVariable("ds_info");
                graphics_model_handle.DirtyVariable("ds_option");
            }
        );
        bind_option(constructor, "wm_option", &new_options.wm_option);
        bind_option(constructor, "ar_option", &new_options.ar_option);
        bind_option(constructor, "hr_option", &new_options.hr_option);
        bind_option(constructor, "msaa_option", &new_options.msaa_option);
        bind_option(constructor, "rr_option", &new_options.rr_option);
        bind_option(constructor, "tf_option", &new_options.tf_option);
        bind_option(constructor, "transparency_option", &new_options.transparency_option);
        // The upscaler options only show while Ray Traced Lighting is on.
        constructor.BindFunc("advanced_lighting_option",
            [](Rml::Variant& out) { get_option(new_options.advanced_lighting_option, out); },
            [](const Rml::Variant& in) {
                set_option(new_options.advanced_lighting_option, in);
                graphics_model_handle.DirtyVariable("options_changed");
                graphics_model_handle.DirtyVariable("advanced_lighting_option");
            }
        );
        bind_option(constructor, "advanced_water_option", &new_options.advanced_water_option);
        bind_option(constructor, "object_culling_option", &new_options.object_culling_option);
        bind_option(constructor, "volumetric_fog_option", &new_options.volumetric_fog_option);
        bind_option(constructor, "god_rays_option", &new_options.god_rays_option);
        bind_option(constructor, "rt_upscaler_option", &new_options.rt_upscaler_option);
        bind_option(constructor, "rt_upscaler_quality_option", &new_options.rt_upscaler_quality_option);
        bind_option(constructor, "rt_light_samples_option", &new_options.rt_light_samples_option);
        bind_option(constructor, "rt_denoiser_option", &new_options.rt_denoiser_option);
        bind_option(constructor, "rt_accumulation_option", &new_options.rt_accumulation_option);
        constructor.BindFunc("rt_upscaler_sharpness",
            [](Rml::Variant& out) {
                out = new_options.rt_upscaler_sharpness;
            },
            [](const Rml::Variant& in) {
                new_options.rt_upscaler_sharpness = std::clamp(in.Get<int>(), 0, 100);
                graphics_model_handle.DirtyVariable("options_changed");
                graphics_model_handle.DirtyVariable("rt_upscaler_sharpness");
            });
        constructor.BindFunc("rr_manual_value",
            [](Rml::Variant& out) {
                out = new_options.rr_manual_value;
            },
            [](const Rml::Variant& in) {
                new_options.rr_manual_value = in.Get<int>();
                graphics_model_handle.DirtyVariable("options_changed");
            });
        constructor.BindFunc("ds_option",
            [](Rml::Variant& out) {
                if (new_options.res_option == ultramodern::renderer::Resolution::Auto) {
                    out = 1;
                } else {
                    out = new_options.ds_option;
                }
            },
            [](const Rml::Variant& in) {
                new_options.ds_option = in.Get<int>();
                graphics_model_handle.DirtyVariable("options_changed");
                graphics_model_handle.DirtyVariable("ds_info");
            });

        constructor.BindFunc("display_refresh_rate",
            [](Rml::Variant& out) {
                out = ultramodern::get_display_refresh_rate();
            });

        constructor.BindFunc("options_changed",
            [](Rml::Variant& out) {
                out = (ultramodern::renderer::get_graphics_config() != new_options);
            });
        constructor.BindFunc("ds_info",
            [](Rml::Variant& out) {
                switch (new_options.res_option) {
                    default:
                    case ultramodern::renderer::Resolution::Auto:
                        out = "Downsampling is not available at auto resolution";
                        return;
                    case ultramodern::renderer::Resolution::Original:
                        if (new_options.ds_option == 2) {
                            out = "Rendered in 480p and scaled to 240p";
                        } else if (new_options.ds_option == 4) {
                            out = "Rendered in 960p and scaled to 240p";
                        }
                        return;
                    case ultramodern::renderer::Resolution::Original2x:
                        if (new_options.ds_option == 2) {
                            out = "Rendered in 960p and scaled to 480p";
                        } else if (new_options.ds_option == 4) {
                            out = "Rendered in 4K and scaled to 480p";
                        }
                        return;
                }
                out = "";
            });
        
        constructor.BindFunc("gfx_help__apply", [](Rml::Variant& out) {
            if (cont_active) {
                out = \
                    (recomp::get_input_binding(recomp::GameInput::APPLY_MENU, 0, recomp::InputDevice::Controller).to_string() != "" ?
                        " " + recomp::get_input_binding(recomp::GameInput::APPLY_MENU, 0, recomp::InputDevice::Controller).to_string() :
                        ""
                    ) + \
                    (recomp::get_input_binding(recomp::GameInput::APPLY_MENU, 1, recomp::InputDevice::Controller).to_string() != "" ?
                        " " + recomp::get_input_binding(recomp::GameInput::APPLY_MENU, 1, recomp::InputDevice::Controller).to_string() :
                        ""
                    );
            } else {
                out = " " PF_KEYBOARD_F;
            }
        });

        constructor.Bind("msaa2x_supported", &msaa2x_supported);
        constructor.Bind("msaa4x_supported", &msaa4x_supported);
        constructor.Bind("msaa8x_supported", &msaa8x_supported);
        constructor.Bind("sample_positions_supported", &sample_positions_supported);

        graphics_model_handle = constructor.GetModelHandle();
    }

    void make_controls_bindings(Rml::Context* context) {
        Rml::DataModelConstructor constructor = context->CreateDataModel("controls_model");
        if (!constructor) {
            throw std::runtime_error("Failed to make RmlUi data model for the controls config menu");
        }

        constructor.BindFunc("input_count", [](Rml::Variant& out) { out = static_cast<uint64_t>(recomp::get_num_inputs()); } );
        constructor.BindFunc("input_device_is_keyboard", [](Rml::Variant& out) { out = cur_device == recomp::InputDevice::Keyboard; } );

        constructor.RegisterTransformFunc("get_input_name", [](const Rml::VariantList& inputs) {
            return Rml::Variant{recomp::get_input_name(static_cast<recomp::GameInput>(inputs.at(0).Get<size_t>()))};
        });

        // The controls style (Modern / Classic): each has its own bindings, so switching changes every row.
        constructor.BindFunc("controls_style", [](Rml::Variant& out) { get_option(recomp::get_controls_style(), out); });
        constructor.BindEventCallback("set_controls_style",
            [](Rml::DataModelHandle model_handle, Rml::Event& event, const Rml::VariantList& inputs) {
                recomp::ControlsStyle style = recomp::get_controls_style();
                set_option(style, inputs.at(0));
                recomp::set_controls_style(style);
                focused_input_index = -1;
                model_handle.DirtyVariable("controls_style");
                model_handle.DirtyVariable("inputs");
                model_handle.DirtyVariable("cur_input_row");
            });

        // Whether the controls page lists an input: the current style's inputs, not the hidden N64 D-pad, and not the PC
        // menu's own inputs for the keyboard. The style and the device are passed only so the rows update with them.
        constructor.RegisterTransformFunc("input_row_visible", [](const Rml::VariantList& inputs) {
            recomp::GameInput input = static_cast<recomp::GameInput>(inputs.at(0).Get<size_t>());
            switch (input) {
                case recomp::GameInput::DPAD_UP:
                case recomp::GameInput::DPAD_DOWN:
                case recomp::GameInput::DPAD_LEFT:
                case recomp::GameInput::DPAD_RIGHT:
                    return Rml::Variant{false};
                case recomp::GameInput::TOGGLE_MENU:
                case recomp::GameInput::ACCEPT_MENU:
                case recomp::GameInput::APPLY_MENU:
                    return Rml::Variant{cur_device != recomp::InputDevice::Keyboard};
                default:
                    return Rml::Variant{recomp::is_input_used_by_style(input, recomp::get_controls_style())};
            }
        });

        constructor.RegisterTransformFunc("get_input_enum_name", [](const Rml::VariantList& inputs) {
            return Rml::Variant{recomp::get_input_enum_name(static_cast<recomp::GameInput>(inputs.at(0).Get<size_t>()))};
        });

        constructor.BindEventCallback("set_input_binding",
            [](Rml::DataModelHandle model_handle, Rml::Event& event, const Rml::VariantList& inputs) {
                scanned_input_index = inputs.at(0).Get<size_t>();
                scanned_binding_index = inputs.at(1).Get<size_t>();
                recomp::start_scanning_input(cur_device);
                model_handle.DirtyVariable("active_binding_input");
                model_handle.DirtyVariable("active_binding_slot");
            });

        constructor.BindEventCallback("reset_input_bindings_to_defaults",
            [](Rml::DataModelHandle model_handle, Rml::Event& event, const Rml::VariantList& inputs) {
                if (cur_device == recomp::InputDevice::Controller) {
                    zelda64::reset_cont_input_bindings();
                } else {
                    zelda64::reset_kb_input_bindings();
                }
                model_handle.DirtyAllVariables();
                nav_help_model_handle.DirtyVariable("nav_help__accept");
                nav_help_model_handle.DirtyVariable("nav_help__exit");
                graphics_model_handle.DirtyVariable("gfx_help__apply");
            });

        constructor.BindEventCallback("clear_input_bindings",
            [](Rml::DataModelHandle model_handle, Rml::Event& event, const Rml::VariantList& inputs) {
                recomp::GameInput input = static_cast<recomp::GameInput>(inputs.at(0).Get<size_t>());
                for (size_t binding_index = 0; binding_index < recomp::bindings_per_input; binding_index++) {
                    recomp::set_input_binding(input, binding_index, cur_device, recomp::InputField{});
                }
                model_handle.DirtyVariable("inputs");
                graphics_model_handle.DirtyVariable("gfx_help__apply");
            });

        constructor.BindEventCallback("reset_single_input_binding_to_default",
            [](Rml::DataModelHandle model_handle, Rml::Event& event, const Rml::VariantList& inputs) {
                recomp::GameInput input = static_cast<recomp::GameInput>(inputs.at(0).Get<size_t>());
                zelda64::reset_single_input_binding(cur_device, input);
                model_handle.DirtyVariable("inputs");
                nav_help_model_handle.DirtyVariable("nav_help__accept");
                nav_help_model_handle.DirtyVariable("nav_help__exit");
            });

        constructor.BindEventCallback("set_input_row_focus",
            [](Rml::DataModelHandle model_handle, Rml::Event& event, const Rml::VariantList& inputs) {
                int input_index = inputs.at(0).Get<size_t>();
                // watch for mouseout being overzealous during event bubbling, only clear if the event's attached element matches the current
                if (input_index == -1 && event.GetType() == "mouseout" && event.GetCurrentElement() != event.GetTargetElement()) {
                    return;
                }
                focused_input_index = input_index;
                model_handle.DirtyVariable("cur_input_row");
                model_handle.DirtyVariable("inputs");   // the picture lights the highlighted row's controls (inputs.pad)
            });

        // Rml variable definition for an individual InputField.
        struct InputFieldVariableDefinition : public Rml::VariableDefinition {
            InputFieldVariableDefinition() : Rml::VariableDefinition(Rml::DataVariableType::Scalar) {}

            virtual bool Get(void* ptr, Rml::Variant& variant) override { variant = reinterpret_cast<recomp::InputField*>(ptr)->to_string(); return true; }
            virtual bool Set(void* ptr, const Rml::Variant& variant) override { return false; }
        };
        // Static instance of the InputField variable definition to have a pointer to return to RmlUi.
        static InputFieldVariableDefinition input_field_definition_instance{};

        // Rml variable definition for an array of InputField values (e.g. all the bindings for a single input).
        struct BindingContainerVariableDefinition : public Rml::VariableDefinition {
            BindingContainerVariableDefinition() : Rml::VariableDefinition(Rml::DataVariableType::Array) {}

            virtual bool Get(void* ptr, Rml::Variant& variant) override { return false; }
            virtual bool Set(void* ptr, const Rml::Variant& variant) override { return false; }

            virtual int Size(void* ptr) override { return recomp::bindings_per_input; }
            virtual Rml::DataVariable Child(void* ptr, const Rml::DataAddressEntry& address) override {
                recomp::GameInput input = static_cast<recomp::GameInput>((uintptr_t)ptr);
                return Rml::DataVariable{&input_field_definition_instance, &recomp::get_input_binding(input, address.index, cur_device)};
            }
        };
        // Static instance of the InputField array variable definition to have a fixed pointer to return to RmlUi.
        static BindingContainerVariableDefinition binding_container_var_instance{};

        // Rml variable definition for an array of an array of InputField values (e.g. all the bindings for all inputs).
        struct BindingArrayContainerVariableDefinition : public Rml::VariableDefinition {
            BindingArrayContainerVariableDefinition() : Rml::VariableDefinition(Rml::DataVariableType::Array) {}

            virtual bool Get(void* ptr, Rml::Variant& variant) override { return false; }
            virtual bool Set(void* ptr, const Rml::Variant& variant) override { return false; }

            virtual int Size(void* ptr) override { return recomp::get_num_inputs(); }
            virtual Rml::DataVariable Child(void* ptr, const Rml::DataAddressEntry& address) override {
                // Encode the input index as the pointer to avoid needing to do any allocations.
                return Rml::DataVariable(&binding_container_var_instance, (void*)(uintptr_t)address.index);
            }
        };

        // Static instance of the BindingArrayContainerVariableDefinition variable definition to have a fixed pointer to return to RmlUi.
        static BindingArrayContainerVariableDefinition binding_array_var_instance{};

        struct InputContainerVariableDefinition : public Rml::VariableDefinition {
            InputContainerVariableDefinition() : Rml::VariableDefinition(Rml::DataVariableType::Struct) {}

            virtual bool Get(void* ptr, Rml::Variant& variant) override { return true; }
            virtual bool Set(void* ptr, const Rml::Variant& variant) override { return false; }

            virtual int Size(void* ptr) override { return recomp::get_num_inputs(); }
            virtual Rml::DataVariable Child(void* ptr, const Rml::DataAddressEntry& address) override {
                if (address.name == "array") {
                    return Rml::DataVariable(&binding_array_var_instance, nullptr);
                }
                else if (address.name == "pad") {
                    return Rml::DataVariable(pad_view_definition, pad_get());
                }
                else {
                    recomp::GameInput input = recomp::get_input_from_enum_name(address.name);
                    if (input != recomp::GameInput::COUNT) {
                        return Rml::DataVariable(&binding_container_var_instance, (void*)(uintptr_t)input);
                    }
                }
                return Rml::DataVariable{};
            }
        };

        // The picture's pad view (PadView), reached as inputs.pad so that it updates with the bindings.
        if (auto line_handle = constructor.RegisterStruct<PadLine>()) {
            line_handle.RegisterMember("text", &PadLine::text);
        }
        constructor.RegisterArray<Rml::Vector<PadLine>>();
        if (auto row_handle = constructor.RegisterStruct<PadRow>()) {
            row_handle.RegisterMember("glyph", &PadRow::glyph);
            row_handle.RegisterMember("glyph_is_text", &PadRow::glyph_is_text);
            row_handle.RegisterMember("lines", &PadRow::lines);
            row_handle.RegisterMember("lit", &PadRow::lit);
        }
        constructor.RegisterArray<Rml::Vector<PadRow>>();
        if (auto spots_handle = constructor.RegisterStruct<PadSpots>()) {
            spots_handle.RegisterMember("lt", &PadSpots::lt);
            spots_handle.RegisterMember("lb", &PadSpots::lb);
            spots_handle.RegisterMember("rt", &PadSpots::rt);
            spots_handle.RegisterMember("rb", &PadSpots::rb);
            spots_handle.RegisterMember("view", &PadSpots::view);
            spots_handle.RegisterMember("menu", &PadSpots::menu);
            spots_handle.RegisterMember("ls", &PadSpots::ls);
            spots_handle.RegisterMember("l3", &PadSpots::l3);
            spots_handle.RegisterMember("rs", &PadSpots::rs);
            spots_handle.RegisterMember("r3", &PadSpots::r3);
            spots_handle.RegisterMember("dpad_up", &PadSpots::dpad_up);
            spots_handle.RegisterMember("dpad_down", &PadSpots::dpad_down);
            spots_handle.RegisterMember("dpad_left", &PadSpots::dpad_left);
            spots_handle.RegisterMember("dpad_right", &PadSpots::dpad_right);
            spots_handle.RegisterMember("a", &PadSpots::a);
            spots_handle.RegisterMember("b", &PadSpots::b);
            spots_handle.RegisterMember("x", &PadSpots::x);
            spots_handle.RegisterMember("y", &PadSpots::y);
        }
        if (auto view_handle = constructor.RegisterStruct<PadView>()) {
            view_handle.RegisterMember("left", &PadView::left);
            view_handle.RegisterMember("right", &PadView::right);
            view_handle.RegisterMember("left_stick", &PadView::left_stick);
            view_handle.RegisterMember("dpad", &PadView::dpad);
            view_handle.RegisterMember("right_stick", &PadView::right_stick);
            view_handle.RegisterMember("face", &PadView::face);
            view_handle.RegisterMember("spots", &PadView::spots);
        }
        pad_view_definition = constructor.GetDataTypeRegister()->GetDefinition<PadView>();

        // Dummy type to associate with the variable definition.
        struct InputContainer {};
        constructor.RegisterCustomDataVariableDefinition<InputContainer>(Rml::MakeUnique<InputContainerVariableDefinition>());

        // Dummy instance of the dummy type to bind to the variable.
        static InputContainer dummy_container;
        constructor.Bind("inputs", &dummy_container);

        constructor.BindFunc("cur_input_row", [](Rml::Variant& out) {
            if (focused_input_index == -1) {
                out = "NONE";
            }
            else {
                out = recomp::get_input_enum_name(static_cast<recomp::GameInput>(focused_input_index));
            }
        });

        constructor.BindFunc("active_binding_input", [](Rml::Variant& out) {
            if (scanned_input_index == -1) {
                out = "NONE";
            }
            else {
                out = recomp::get_input_enum_name(static_cast<recomp::GameInput>(scanned_input_index));
            }
        });

        constructor.Bind<int>("active_binding_slot", &scanned_binding_index);

        controls_model_handle = constructor.GetModelHandle();
    }

    void make_nav_help_bindings(Rml::Context* context) {
        Rml::DataModelConstructor constructor = context->CreateDataModel("nav_help_model");
        if (!constructor) {
            throw std::runtime_error("Failed to make RmlUi data model for nav help");
        }

        constructor.BindFunc("nav_help__navigate", [](Rml::Variant& out) {
            if (cont_active) {
                out = PF_DPAD;
            } else {
                out = PF_KEYBOARD_ARROWS PF_KEYBOARD_TAB;
            }
        });

        constructor.BindFunc("nav_help__accept", [](Rml::Variant& out) {
            if (cont_active) {
                out = \
                    recomp::get_input_binding(recomp::GameInput::ACCEPT_MENU, 0, recomp::InputDevice::Controller).to_string() + \
                    recomp::get_input_binding(recomp::GameInput::ACCEPT_MENU, 1, recomp::InputDevice::Controller).to_string();
            } else {
                out = PF_KEYBOARD_ENTER;
            }
        });

        constructor.BindFunc("nav_help__exit", [](Rml::Variant& out) {
            if (cont_active) {
                out = \
                    recomp::get_input_binding(recomp::GameInput::TOGGLE_MENU, 0, recomp::InputDevice::Controller).to_string() + \
                    recomp::get_input_binding(recomp::GameInput::TOGGLE_MENU, 1, recomp::InputDevice::Controller).to_string();
            } else {
                out = PF_KEYBOARD_ESCAPE;
            }
        });

        nav_help_model_handle = constructor.GetModelHandle();
    }

    void make_general_bindings(Rml::Context* context) {
        Rml::DataModelConstructor constructor = context->CreateDataModel("general_model");
        if (!constructor) {
            throw std::runtime_error("Failed to make RmlUi data model for the control options menu");
        }

        bind_config_list_events(constructor);
        
        constructor.Bind("rumble_strength", &control_options_context.rumble_strength);
        constructor.Bind("gyro_sensitivity", &control_options_context.gyro_sensitivity);
        constructor.Bind("mouse_sensitivity", &control_options_context.mouse_sensitivity);
        constructor.Bind("joystick_deadzone", &control_options_context.joystick_deadzone);
        bind_option(constructor, "targeting_mode", &control_options_context.targeting_mode);
        bind_option(constructor, "background_input_mode", &control_options_context.background_input_mode);
        bind_option(constructor, "film_grain_mode", &control_options_context.film_grain_mode);
        bind_option(constructor, "rt_debug_keys_mode", &control_options_context.rt_debug_keys_mode);
        bind_option(constructor, "radio_comm_box_mode", &control_options_context.radio_comm_box_mode);
        bind_option(constructor, "invert_y_axis_mode", &control_options_context.invert_y_axis_mode);
        bind_option(constructor, "analog_camera_invert_mode", &control_options_context.analog_camera_invert_mode);
        constructor.Bind("camera_turn_speed", &control_options_context.camera_turn_speed);
        constructor.Bind("camera_tilt_speed", &control_options_context.camera_tilt_speed);
        bind_option(constructor, "camera_invert_turn_mode", &control_options_context.camera_invert_turn_mode);
        bind_option(constructor, "camera_invert_tilt_mode", &control_options_context.camera_invert_tilt_mode);

        general_model_handle = constructor.GetModelHandle();
    }
    
    void make_sound_options_bindings(Rml::Context* context) {
        Rml::DataModelConstructor constructor = context->CreateDataModel("sound_options_model");
        if (!constructor) {
            throw std::runtime_error("Failed to make RmlUi data model for the sound options menu");
        }

        bind_config_list_events(constructor);
        
        sound_options_model_handle = constructor.GetModelHandle();

        bind_atomic(constructor, sound_options_model_handle, "main_volume", &sound_options_context.main_volume);
        bind_atomic(constructor, sound_options_model_handle, "bgm_volume", &sound_options_context.bgm_volume);
        bind_atomic(constructor, sound_options_model_handle, "sfx_volume", &sound_options_context.sfx_volume);
		bind_atomic(constructor, sound_options_model_handle, "voice_volume", &sound_options_context.voice_volume);
        bind_atomic(constructor, sound_options_model_handle, "low_health_beeps_enabled", &sound_options_context.low_health_beeps_enabled);
    }

    void make_debug_bindings(Rml::Context* context) {
        Rml::DataModelConstructor constructor = context->CreateDataModel("debug_model");
        if (!constructor) {
            throw std::runtime_error("Failed to make RmlUi data model for the debug menu");
        }

        bind_config_list_events(constructor);

        // Bind the debug mode enabled flag.
        constructor.Bind("debug_enabled", &debug_context.debug_enabled);
        
        // Register the array type for string vectors.
        constructor.RegisterArray<std::vector<std::string>>();
        
        // Bind the warp parameter indices
        constructor.Bind("area_index", &debug_context.area_index);
        constructor.Bind("scene_index", &debug_context.scene_index);
        constructor.Bind("entrance_index", &debug_context.entrance_index);

        // Bind the vectors for warp names
        constructor.Bind("area_names", &debug_context.area_names);
        constructor.Bind("scene_names", &debug_context.scene_names);
        constructor.Bind("entrance_names", &debug_context.entrance_names);

        constructor.Bind("debug_time_day", &debug_context.set_time_day);
        constructor.Bind("debug_time_hour", &debug_context.set_time_hour);
        constructor.Bind("debug_time_minute", &debug_context.set_time_minute);

        debug_context.model_handle = constructor.GetModelHandle();
    }

    void make_bindings(Rml::Context* context) override {
        // initially set cont state for ui help
        //recomp::config_menu_set_cont_or_kb(recompui::get_cont_active());
        make_nav_help_bindings(context);
        make_general_bindings(context);
        make_controls_bindings(context);
        make_graphics_bindings(context);
        make_sound_options_bindings(context);
        make_debug_bindings(context);
    }
};

std::unique_ptr<recompui::MenuController> recompui::create_config_menu() {
    return std::make_unique<ConfigMenu>();
}

bool zelda64::get_debug_mode_enabled() {
    return debug_context.debug_enabled;
}

void zelda64::set_debug_mode_enabled(bool enabled) {
    debug_context.debug_enabled = enabled;
    if (debug_context.model_handle) {
        debug_context.model_handle.DirtyVariable("debug_enabled");
    }
}

void recompui::update_supported_options() {
    msaa2x_supported = zelda64::renderer::RT64MaxMSAA() >= RT64::UserConfiguration::Antialiasing::MSAA2X;
    msaa4x_supported = zelda64::renderer::RT64MaxMSAA() >= RT64::UserConfiguration::Antialiasing::MSAA4X;
    msaa8x_supported = zelda64::renderer::RT64MaxMSAA() >= RT64::UserConfiguration::Antialiasing::MSAA8X;
    sample_positions_supported = zelda64::renderer::RT64SamplePositionsSupported();
    
    new_options = ultramodern::renderer::get_graphics_config();

    graphics_model_handle.DirtyAllVariables();
}

void recompui::toggle_fullscreen() {
    new_options.wm_option = (new_options.wm_option == ultramodern::renderer::WindowMode::Windowed) ? ultramodern::renderer::WindowMode::Fullscreen : ultramodern::renderer::WindowMode::Windowed;
    apply_graphics_config();
    graphics_model_handle.DirtyVariable("wm_option");
}

void recompui::set_config_tab(ConfigTab tab) {
    get_config_tabset()->SetActiveTab(config_tab_to_index(tab));
}

Rml::ElementTabSet* recompui::get_config_tabset() {
    ContextId config_context = recompui::get_config_context_id();

    ContextId old_context = recompui::try_close_current_context();

    Rml::ElementDocument *doc = config_context.get_document();
    assert(doc != nullptr);

    Rml::Element *tabset_el = doc->GetElementById("config_tabset");
    assert(tabset_el != nullptr);

    Rml::ElementTabSet *tabset = rmlui_dynamic_cast<Rml::ElementTabSet *>(tabset_el);
    assert(tabset != nullptr);

    if (old_context != ContextId::null()) {
        old_context.open();
    }

    return tabset;
}

Rml::Element* recompui::get_mod_tab() {
    ContextId config_context = recompui::get_config_context_id();

    ContextId old_context = recompui::try_close_current_context();

    Rml::ElementDocument* doc = config_context.get_document();
    assert(doc != nullptr);

    Rml::Element* tab_el = doc->GetElementById("tab_mods");
    assert(tab_el != nullptr);

    if (old_context != ContextId::null()) {
        old_context.open();
    }

    return tab_el;
}
