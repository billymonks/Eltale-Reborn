#ifndef __RECOMP_INPUT_H__
#define __RECOMP_INPUT_H__

#include <cstdint>
#include <variant>
#include <vector>
#include <type_traits>
#include <span>
#include <string>
#include <string_view>

#include "ultramodern/input.hpp"

#include "json/json.hpp"

namespace recomp {
    // x-macros to build input enums and arrays.
    // First parameter is the enum name, second parameter is the bit field for the input (or 0 if there is no associated one), third is the readable name.
    // Readable names say what an input does, not which button it's on: the bindings are the player's to change.
    // The enum names are the keys in controls.json, so they never change; the order is the controls page's list order.
    // The N64 button inputs must stay first and contiguous (GameInput::N64_BUTTON_START, controls.cpp).
    #define DEFINE_N64_BUTTON_INPUTS() \
        DEFINE_INPUT(A, 0x8000, "Confirm, talk, attack") \
        DEFINE_INPUT(B, 0x4000, "Cancel") \
        DEFINE_INPUT(R, 0x0010, "Item menu") \
        DEFINE_INPUT(SPRINT, 0x0040, "Sprint") \
        DEFINE_INPUT(START, 0x1000, "Status and map") \
        DEFINE_INPUT(C_UP, 0x0008, "Spell: Fire") \
        DEFINE_INPUT(C_LEFT, 0x0002, "Spell: Earth") \
        DEFINE_INPUT(C_DOWN, 0x0004, "Spell: Water") \
        DEFINE_INPUT(C_RIGHT, 0x0001, "Spell: Wind") \
        DEFINE_INPUT(L, 0x0020, "Items left, camera angle") \
        DEFINE_INPUT(Z, 0x2000, "Items right, also confirm, pass turn") \
        DEFINE_INPUT(DPAD_UP, 0x0800, "N64 D-pad Up") \
        DEFINE_INPUT(DPAD_RIGHT, 0x0100, "N64 D-pad Right") \
        DEFINE_INPUT(DPAD_DOWN, 0x0400, "N64 D-pad Down") \
        DEFINE_INPUT(DPAD_LEFT, 0x0200, "N64 D-pad Left")

    #define DEFINE_N64_AXIS_INPUTS() \
        DEFINE_INPUT(Y_AXIS_POS, 0, "Move up") \
        DEFINE_INPUT(Y_AXIS_NEG, 0, "Move down") \
        DEFINE_INPUT(X_AXIS_NEG, 0, "Move left") \
        DEFINE_INPUT(X_AXIS_POS, 0, "Move right") \

    // Actions with no N64 button. The game's patches read them through recomp_get_action_input (recomp_api.cpp),
    // by the ids in GameAction below.
    #define DEFINE_ACTION_INPUTS() \
        DEFINE_INPUT(CAMERA_TARGET, 0, "Focus camera") \
        DEFINE_INPUT(CAMERA_ZOOM, 0, "Zoom out") \
        DEFINE_INPUT(PAGE_PREV, 0, "Previous page, item") \
        DEFINE_INPUT(PAGE_NEXT, 0, "Next page, item") \
        DEFINE_INPUT(CAMERA_LEFT, 0, "Camera left") \
        DEFINE_INPUT(CAMERA_RIGHT, 0, "Camera right") \
        DEFINE_INPUT(CAMERA_UP, 0, "Camera up") \
        DEFINE_INPUT(CAMERA_DOWN, 0, "Camera down")

    #define DEFINE_RECOMP_UI_INPUTS() \
        DEFINE_INPUT(TOGGLE_MENU, 0, "Open the PC menu") \
        DEFINE_INPUT(ACCEPT_MENU, 0, "Accept (PC menu)") \
        DEFINE_INPUT(APPLY_MENU, 0, "Apply (PC menu)")

    #define DEFINE_ALL_INPUTS() \
        DEFINE_N64_BUTTON_INPUTS() \
        DEFINE_N64_AXIS_INPUTS() \
        DEFINE_ACTION_INPUTS() \
        DEFINE_RECOMP_UI_INPUTS()

    // Stable ids for the actions the game's patches (and mods) can read with recomp_get_action_input. Append only.
    enum class GameAction : uint32_t {
        CameraTarget = 0,
        CameraZoom = 1,
        PagePrev = 2,
        PageNext = 3,
        Count
    };

    // Enum containing every recomp input.
    #define DEFINE_INPUT(name, value, readable) name,
    enum class GameInput {
        DEFINE_ALL_INPUTS()

        COUNT,
        N64_BUTTON_START = A,
        N64_BUTTON_COUNT = C_RIGHT - N64_BUTTON_START + 1,
        N64_AXIS_START = X_AXIS_NEG,
        N64_AXIS_COUNT = Y_AXIS_POS - N64_AXIS_START + 1,
    };
    #undef DEFINE_INPUT

    struct InputField {
        uint32_t input_type;
        int32_t input_id;
        std::string to_string() const;
        auto operator<=>(const InputField& rhs) const = default;
    };

    void poll_inputs();
    float get_input_analog(const InputField& field);
    float get_input_analog(const std::span<const recomp::InputField> fields);
    bool get_input_digital(const InputField& field);
    bool get_input_digital(const std::span<const recomp::InputField> fields);
    void get_gyro_deltas(float* x, float* y);
    void get_mouse_deltas(float* x, float* y);
    void get_right_analog(float* x, float* y);

    // A controller button, or one direction of a stick or trigger, as a binding (by SDL's button and axis ids).
    InputField controller_button_field(int sdl_button);
    InputField controller_axis_field(int sdl_axis, bool positive);

    enum class InputDevice {
        Controller,
        Keyboard,
        COUNT
    };

    void start_scanning_input(InputDevice device);
    void stop_scanning_input();
    void finish_scanning_input(InputField scanned_field);
    void cancel_scanning_input();
    void config_menu_set_cont_or_kb(bool cont_interacted);
    InputField get_scanned_input();
    int get_scanned_input_index();
    
    struct DefaultN64Mappings {
        std::vector<InputField> a;
        std::vector<InputField> b;
        std::vector<InputField> l;
        std::vector<InputField> r;
        std::vector<InputField> z;
        std::vector<InputField> start;

        std::vector<InputField> c_left;
        std::vector<InputField> c_right;
        std::vector<InputField> c_up;
        std::vector<InputField> c_down;

        std::vector<InputField> dpad_left;
        std::vector<InputField> dpad_right;
        std::vector<InputField> dpad_up;
        std::vector<InputField> dpad_down;

        std::vector<InputField> analog_left;
        std::vector<InputField> analog_right;
        std::vector<InputField> analog_up;
        std::vector<InputField> analog_down;

        std::vector<InputField> toggle_menu;
        std::vector<InputField> accept_menu;
        std::vector<InputField> apply_menu;

        std::vector<InputField> sprint;

        std::vector<InputField> camera_target;
        std::vector<InputField> camera_zoom;
        std::vector<InputField> page_prev;
        std::vector<InputField> page_next;
        std::vector<InputField> camera_left;
        std::vector<InputField> camera_right;
        std::vector<InputField> camera_up;
        std::vector<InputField> camera_down;
    };

    inline const std::vector<InputField>& get_default_mapping_for_input(const DefaultN64Mappings& defaults, const GameInput input) {
        static const std::vector<InputField> empty_input_field{};
        switch (input) {
            case GameInput::A: return defaults.a;
            case GameInput::B: return defaults.b;
            case GameInput::L: return defaults.l;
            case GameInput::R: return defaults.r;
            case GameInput::Z: return defaults.z;
            case GameInput::START: return defaults.start;
            case GameInput::C_LEFT: return defaults.c_left;
            case GameInput::C_RIGHT: return defaults.c_right;
            case GameInput::C_UP: return defaults.c_up;
            case GameInput::C_DOWN: return defaults.c_down;
            case GameInput::DPAD_LEFT: return defaults.dpad_left;
            case GameInput::DPAD_RIGHT: return defaults.dpad_right;
            case GameInput::DPAD_UP: return defaults.dpad_up;
            case GameInput::DPAD_DOWN: return defaults.dpad_down;
            case GameInput::X_AXIS_NEG: return defaults.analog_left;
            case GameInput::X_AXIS_POS: return defaults.analog_right;
            case GameInput::Y_AXIS_POS: return defaults.analog_up;
            case GameInput::Y_AXIS_NEG: return defaults.analog_down;
            case GameInput::TOGGLE_MENU: return defaults.toggle_menu;
            case GameInput::ACCEPT_MENU: return defaults.accept_menu;
            case GameInput::APPLY_MENU: return defaults.apply_menu;
            case GameInput::SPRINT: return defaults.sprint;
            case GameInput::CAMERA_TARGET: return defaults.camera_target;
            case GameInput::CAMERA_ZOOM: return defaults.camera_zoom;
            case GameInput::PAGE_PREV: return defaults.page_prev;
            case GameInput::PAGE_NEXT: return defaults.page_next;
            case GameInput::CAMERA_LEFT: return defaults.camera_left;
            case GameInput::CAMERA_RIGHT: return defaults.camera_right;
            case GameInput::CAMERA_UP: return defaults.camera_up;
            case GameInput::CAMERA_DOWN: return defaults.camera_down;
            default: return empty_input_field;
        }
    }

    // Modern controls' defaults.
    extern const DefaultN64Mappings default_n64_keyboard_mappings;
    extern const DefaultN64Mappings default_n64_controller_mappings;
    // Classic controls' defaults: Eltale Reborn's original layouts, as released (a Dreamcast pad's, and the keyboard's).
    extern const DefaultN64Mappings classic_n64_controller_mappings;
    extern const DefaultN64Mappings classic_n64_keyboard_mappings;

    // The controls style (chosen on the Controls page). Classic is Eltale Reborn's original controls, unchanged: its
    // layout, N64 L and Z with all their jobs, hold B to follow, the original camera. Modern is the modern controls:
    // the actions above (N64 L and Z aren't bindable), the free camera and the camera fixes. Each style keeps its own
    // bindings, so switching never loses either.
    enum class ControlsStyle {
        Classic,
        Modern,
        OptionCount
    };
    NLOHMANN_JSON_SERIALIZE_ENUM(recomp::ControlsStyle, {
        {recomp::ControlsStyle::Classic, "Classic"},
        {recomp::ControlsStyle::Modern, "Modern"}
    });
    // The style a new install starts in. Classic keeps the game as released until a player picks Modern on the Controls
    // page; ControlsStyle::Modern would start new players on the modern controls. (A controls.json from before the
    // styles always loads as Classic, with its own bindings.)
    constexpr ControlsStyle default_controls_style = ControlsStyle::Classic;
    ControlsStyle get_controls_style();
    void set_controls_style(ControlsStyle style);
    // A style's default bindings for a device.
    const DefaultN64Mappings& get_default_mappings(InputDevice device, ControlsStyle style);
    // Whether an input is the player's to bind in a style (Classic: the N64 buttons; Modern: everything but N64 L and Z).
    bool is_input_used_by_style(GameInput input, ControlsStyle style);
    // The PC menu's own inputs (open it, accept, apply): one set of bindings for both styles, so switching styles never
    // moves the menu's buttons.
    bool is_input_shared_by_styles(GameInput input);

    constexpr size_t bindings_per_input = 2;

    size_t get_num_inputs();
    const std::string& get_input_name(GameInput input);
    const std::string& get_input_enum_name(GameInput input);
    GameInput get_input_from_enum_name(const std::string_view name);
    // The current style's bindings; the overloads with a style reach either style's.
    InputField& get_input_binding(GameInput input, size_t binding_index, InputDevice device);
    void set_input_binding(GameInput input, size_t binding_index, InputDevice device, InputField value);
    InputField& get_input_binding(GameInput input, size_t binding_index, InputDevice device, ControlsStyle style);
    void set_input_binding(GameInput input, size_t binding_index, InputDevice device, ControlsStyle style, InputField value);

    bool get_n64_input(int controller_num, uint16_t* buttons_out, float* x_out, float* y_out);
    // An action's value from its controller and keyboard bindings: 0 or 1 for buttons and keys, 0..1 for a trigger or
    // a stick direction. 0 while game input is disabled (the PC menu is open).
    float get_action_input(GameAction action);
    // The camera stick (CAMERA_LEFT/RIGHT/UP/DOWN), each axis -1..1, the controller's dead zone applied.
    void get_camera_stick(float* x, float* y);
    void set_rumble(int controller_num, bool);
    void update_rumble();
    void handle_events();

    ultramodern::input::connected_device_info_t get_connected_device_info(int controller_num);
    
    // Rumble strength ranges from 0 to 100.
    int get_rumble_strength();
    void set_rumble_strength(int strength);
    
    // Gyro and mouse sensitivities range from 0 to 100.
    int get_gyro_sensitivity();
    int get_mouse_sensitivity();
    int get_joystick_deadzone();
    void set_gyro_sensitivity(int strength);
    void set_mouse_sensitivity(int strength);
    void set_joystick_deadzone(int strength);
    void apply_joystick_deadzone(float x_in, float y_in, float* x_out, float* y_out);
    void set_right_analog_suppressed(bool suppressed);

    // The camera stick's speeds, in percent of the tuned speed (100): turning left / right, and tilting up / down.
    int get_camera_turn_speed();
    int get_camera_tilt_speed();
    void set_camera_turn_speed(int speed);
    void set_camera_tilt_speed(int speed);

    enum class BackgroundInputMode {
        On,
        Off,
		OptionCount
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(recomp::BackgroundInputMode, {
        {recomp::BackgroundInputMode::On, "On"},
        {recomp::BackgroundInputMode::Off, "Off"}
    });

    BackgroundInputMode get_background_input_mode();
    void set_background_input_mode(BackgroundInputMode mode);

    bool game_input_disabled();
    bool all_input_disabled();
}

#endif
