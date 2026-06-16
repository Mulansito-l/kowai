#include "KowaiEngine/kowai.h"
#include "KowaiEngine/engine_internal.h"

namespace Kowai::Input
{
    void set_context(const char* name, bool active)
    {
        KowaiEngine* e = kowai_get_current_engine();
        if (!e) return;
        kowai_input_set_context_active(&e->input_system, name, active);
    }

    bool action(const char* action_name)
    {
        KowaiEngine* e = kowai_get_current_engine();
        if (!e) return false;
        return kowai_input_get_action(&e->input_system, action_name);
    }

    bool action_down(const char* action_name)
    {
        KowaiEngine* e = kowai_get_current_engine();
        if (!e) return false;
        return kowai_input_get_action_down(&e->input_system, action_name);
    }

    bool action_up(const char* action_name)
    {
        KowaiEngine* e = kowai_get_current_engine();
        if (!e) return false;
        return kowai_input_get_action_up(&e->input_system, action_name);
    }

    float axis_left_x() { KowaiEngine* e = kowai_get_current_engine(); return e ? e->input_system.left_stick_x : 0.0f; }
    float axis_left_y() { KowaiEngine* e = kowai_get_current_engine(); return e ? e->input_system.left_stick_y : 0.0f; }
    float axis_right_x() { KowaiEngine* e = kowai_get_current_engine(); return e ? e->input_system.right_stick_x : 0.0f; }
    float axis_right_y() { KowaiEngine* e = kowai_get_current_engine(); return e ? e->input_system.right_stick_y : 0.0f; }
    float trigger_left() { KowaiEngine* e = kowai_get_current_engine(); return e ? e->input_system.left_trigger : 0.0f; }
    float trigger_right() { KowaiEngine* e = kowai_get_current_engine(); return e ? e->input_system.right_trigger : 0.0f; }

    float mouse_delta_x() { KowaiEngine* e = kowai_get_current_engine(); return e ? e->input_system.mouse_delta_x : 0.0f; }
    float mouse_delta_y() { KowaiEngine* e = kowai_get_current_engine(); return e ? e->input_system.mouse_delta_y : 0.0f; }
    float mouse_x() { KowaiEngine* e = kowai_get_current_engine(); return e ? e->input_system.mouse_x : 0.0f; }
    float mouse_y() { KowaiEngine* e = kowai_get_current_engine(); return e ? e->input_system.mouse_y : 0.0f; }
}