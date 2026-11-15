#include <algorithm>
#include <array>
#include <atomic>

#include "librecomp/helpers.hpp"
#include "recomp_input.h"
#include "ultramodern/ultramodern.hpp"

// Arrays that hold the mappings for every input for keyboard and controller respectively, per controls style.
using input_mapping = std::array<recomp::InputField, recomp::bindings_per_input>;
using input_mapping_array = std::array<input_mapping, static_cast<size_t>(recomp::GameInput::COUNT)>;
constexpr size_t num_styles = static_cast<size_t>(recomp::ControlsStyle::OptionCount);
static std::array<input_mapping_array, num_styles> keyboard_style_mappings{};
static std::array<input_mapping_array, num_styles> controller_style_mappings{};
static std::atomic<recomp::ControlsStyle> controls_style{recomp::default_controls_style};

recomp::ControlsStyle recomp::get_controls_style() {
    return controls_style.load();
}

void recomp::set_controls_style(recomp::ControlsStyle style) {
    if (style == recomp::ControlsStyle::Classic || style == recomp::ControlsStyle::Modern) {
        controls_style.store(style);
    }
}

static input_mapping_array& style_mappings(recomp::InputDevice device, recomp::ControlsStyle style) {
    size_t index = std::min(static_cast<size_t>(style), num_styles - 1);
    return (device == recomp::InputDevice::Controller) ? controller_style_mappings[index] : keyboard_style_mappings[index];
}

static input_mapping_array& current_mappings(recomp::InputDevice device) {
    return style_mappings(device, recomp::get_controls_style());
}

const recomp::DefaultN64Mappings& recomp::get_default_mappings(recomp::InputDevice device, recomp::ControlsStyle style) {
    if (style == recomp::ControlsStyle::Classic) {
        return device == recomp::InputDevice::Keyboard ? recomp::classic_n64_keyboard_mappings : recomp::classic_n64_controller_mappings;
    }
    return device == recomp::InputDevice::Keyboard ? recomp::default_n64_keyboard_mappings : recomp::default_n64_controller_mappings;
}

bool recomp::is_input_shared_by_styles(recomp::GameInput input) {
    return input == recomp::GameInput::TOGGLE_MENU || input == recomp::GameInput::ACCEPT_MENU ||
           input == recomp::GameInput::APPLY_MENU;
}

bool recomp::is_input_used_by_style(recomp::GameInput input, recomp::ControlsStyle style) {
    switch (input) {
        // Modern controls' actions cover N64 L and Z.
        case recomp::GameInput::L:
        case recomp::GameInput::Z:
            return style == recomp::ControlsStyle::Classic;
        case recomp::GameInput::CAMERA_TARGET:
        case recomp::GameInput::CAMERA_ZOOM:
        case recomp::GameInput::PAGE_PREV:
        case recomp::GameInput::PAGE_NEXT:
        case recomp::GameInput::CAMERA_LEFT:
        case recomp::GameInput::CAMERA_RIGHT:
        case recomp::GameInput::CAMERA_UP:
        case recomp::GameInput::CAMERA_DOWN:
            return style == recomp::ControlsStyle::Modern;
        default:
            return true;
    }
}

// Make the button value array, which maps a button index to its bit field.
#define DEFINE_INPUT(name, value, readable) uint16_t(value##u),
static const std::array n64_button_values = {
    DEFINE_N64_BUTTON_INPUTS()
};
#undef DEFINE_INPUT

// Make the input name array.
#define DEFINE_INPUT(name, value, readable) readable,
static const std::vector<std::string> input_names = {
    DEFINE_ALL_INPUTS()
};
#undef DEFINE_INPUT

// Make the input enum name array.
#define DEFINE_INPUT(name, value, readable) #name,
static const std::vector<std::string> input_enum_names = {
    DEFINE_ALL_INPUTS()
};
#undef DEFINE_INPUT

size_t recomp::get_num_inputs() {
    return (size_t)GameInput::COUNT;
}

const std::string& recomp::get_input_name(GameInput input) {
    return input_names.at(static_cast<size_t>(input));
}

const std::string& recomp::get_input_enum_name(GameInput input) {
    return input_enum_names.at(static_cast<size_t>(input));
}

recomp::GameInput recomp::get_input_from_enum_name(const std::string_view enum_name) {
    auto find_it = std::find(input_enum_names.begin(), input_enum_names.end(), enum_name);
    if (find_it == input_enum_names.end()) {
        return recomp::GameInput::COUNT;
    }

    return static_cast<recomp::GameInput>(find_it - input_enum_names.begin());
}

// Due to an RmlUi limitation this can't be const. Ideally it would return a const reference or even just a straight up copy.
recomp::InputField& recomp::get_input_binding(GameInput input, size_t binding_index, recomp::InputDevice device) {
    return recomp::get_input_binding(input, binding_index, device, recomp::get_controls_style());
}

void recomp::set_input_binding(recomp::GameInput input, size_t binding_index, recomp::InputDevice device, recomp::InputField value) {
    recomp::set_input_binding(input, binding_index, device, recomp::get_controls_style(), value);
}

recomp::InputField& recomp::get_input_binding(GameInput input, size_t binding_index, recomp::InputDevice device, recomp::ControlsStyle style) {
    if (recomp::is_input_shared_by_styles(input)) {
        style = recomp::ControlsStyle::Modern;
    }
    input_mapping_array& device_mappings = style_mappings(device, style);
    input_mapping& cur_input_mapping = device_mappings.at(static_cast<size_t>(input));

    if (binding_index < cur_input_mapping.size()) {
        return cur_input_mapping[binding_index];
    }
    else {
        static recomp::InputField dummy_field = {};
        return dummy_field;
    }
}

void recomp::set_input_binding(recomp::GameInput input, size_t binding_index, recomp::InputDevice device, recomp::ControlsStyle style, recomp::InputField value) {
    if (recomp::is_input_shared_by_styles(input)) {
        style = recomp::ControlsStyle::Modern;
    }
    input_mapping_array& device_mappings = style_mappings(device, style);
    input_mapping& cur_input_mapping = device_mappings.at(static_cast<size_t>(input));

    if (binding_index < cur_input_mapping.size()) {
        cur_input_mapping[binding_index] = value;
    }
}

static recomp::GameInput action_to_input(recomp::GameAction action) {
    switch (action) {
        case recomp::GameAction::CameraTarget: return recomp::GameInput::CAMERA_TARGET;
        case recomp::GameAction::CameraZoom: return recomp::GameInput::CAMERA_ZOOM;
        case recomp::GameAction::PagePrev: return recomp::GameInput::PAGE_PREV;
        case recomp::GameAction::PageNext: return recomp::GameInput::PAGE_NEXT;
        default: return recomp::GameInput::COUNT;
    }
}

static float input_value(recomp::GameInput input) {
    size_t index = static_cast<size_t>(input);
    return std::clamp(recomp::get_input_analog(current_mappings(recomp::InputDevice::Controller)[index]) +
                      recomp::get_input_analog(current_mappings(recomp::InputDevice::Keyboard)[index]), 0.0f, 1.0f);
}

float recomp::get_action_input(recomp::GameAction action) {
    recomp::GameInput input = action_to_input(action);
    if (input == recomp::GameInput::COUNT || recomp::game_input_disabled() ||
        !recomp::is_input_used_by_style(input, recomp::get_controls_style())) {
        return 0.0f;
    }
    return input_value(input);
}

void recomp::get_camera_stick(float* x_out, float* y_out) {
    *x_out = 0.0f;
    *y_out = 0.0f;
    if (recomp::game_input_disabled() || recomp::get_controls_style() != recomp::ControlsStyle::Modern) {
        return;
    }
    // Y is positive down, as SDL's axes are.
    auto device_value = [](const input_mapping_array& mappings, recomp::GameInput input) {
        return recomp::get_input_analog(mappings[static_cast<size_t>(input)]);
    };
    float cx = device_value(current_mappings(recomp::InputDevice::Controller), GameInput::CAMERA_RIGHT) - device_value(current_mappings(recomp::InputDevice::Controller), GameInput::CAMERA_LEFT);
    float cy = device_value(current_mappings(recomp::InputDevice::Controller), GameInput::CAMERA_DOWN) - device_value(current_mappings(recomp::InputDevice::Controller), GameInput::CAMERA_UP);
    recomp::apply_joystick_deadzone(cx, cy, &cx, &cy);
    float kx = device_value(current_mappings(recomp::InputDevice::Keyboard), GameInput::CAMERA_RIGHT) - device_value(current_mappings(recomp::InputDevice::Keyboard), GameInput::CAMERA_LEFT);
    float ky = device_value(current_mappings(recomp::InputDevice::Keyboard), GameInput::CAMERA_DOWN) - device_value(current_mappings(recomp::InputDevice::Keyboard), GameInput::CAMERA_UP);
    *x_out = std::clamp(cx + kx, -1.0f, 1.0f);
    *y_out = std::clamp(cy + ky, -1.0f, 1.0f);
}

bool recomp::get_n64_input(int controller_num, uint16_t* buttons_out, float* x_out, float* y_out) {
    uint16_t cur_buttons = 0;
    float cur_x = 0.0f;
    float cur_y = 0.0f;
    
    if (controller_num != 0) {
        return false;
    }

    if (!recomp::game_input_disabled()) {
        for (size_t i = 0; i < n64_button_values.size(); i++) {
            size_t input_index = (size_t)GameInput::N64_BUTTON_START + i;
            if (!recomp::is_input_used_by_style(static_cast<GameInput>(input_index), recomp::get_controls_style())) {
                continue;
            }
            cur_buttons |= recomp::get_input_digital(current_mappings(recomp::InputDevice::Keyboard)[input_index]) ? n64_button_values[i] : 0;
            cur_buttons |= recomp::get_input_digital(current_mappings(recomp::InputDevice::Controller)[input_index]) ? n64_button_values[i] : 0;
        }

        float joystick_deadzone = recomp::get_joystick_deadzone() / 100.0f;

        float joystick_x = recomp::get_input_analog(current_mappings(recomp::InputDevice::Controller)[(size_t)GameInput::X_AXIS_POS])
                        - recomp::get_input_analog(current_mappings(recomp::InputDevice::Controller)[(size_t)GameInput::X_AXIS_NEG]);

        float joystick_y = recomp::get_input_analog(current_mappings(recomp::InputDevice::Controller)[(size_t)GameInput::Y_AXIS_POS])
                        - recomp::get_input_analog(current_mappings(recomp::InputDevice::Controller)[(size_t)GameInput::Y_AXIS_NEG]);

        recomp::apply_joystick_deadzone(joystick_x, joystick_y, &joystick_x, &joystick_y);

        cur_x = recomp::get_input_analog(current_mappings(recomp::InputDevice::Keyboard)[(size_t)GameInput::X_AXIS_POS])
                - recomp::get_input_analog(current_mappings(recomp::InputDevice::Keyboard)[(size_t)GameInput::X_AXIS_NEG]) + joystick_x;

        cur_y = recomp::get_input_analog(current_mappings(recomp::InputDevice::Keyboard)[(size_t)GameInput::Y_AXIS_POS])
                - recomp::get_input_analog(current_mappings(recomp::InputDevice::Keyboard)[(size_t)GameInput::Y_AXIS_NEG]) + joystick_y;
    }

    *buttons_out = cur_buttons;
    *x_out = std::clamp(cur_x * 0.65f, -1.0f, 1.0f);
    *y_out = std::clamp(cur_y * 0.65f, -1.0f, 1.0f);

    return true;
}
