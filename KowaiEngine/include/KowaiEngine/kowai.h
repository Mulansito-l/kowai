#ifndef KOWAI_H
#define KOWAI_H

extern "C" {
    #include "KowaiEngine/engine_internal.h"
    #include "KowaiEngine/input.h"
}

namespace Kowai
{
    namespace Input
    {
        void set_context(const char* name, bool active);

        bool action(const char* action_name);
        bool action_down(const char* action_name);
        bool action_up(const char* action_name);

        float axis_left_x();
        float axis_left_y();
        float axis_right_x();
        float axis_right_y();
        float trigger_left();
        float trigger_right();

        float mouse_delta_x();
        float mouse_delta_y();
        float mouse_x();
        float mouse_y();
    }
}

#endif // KOWAI_H