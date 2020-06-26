#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include "types.h"

void run_command(const char* command, char *output, s32 output_size)
{
    output[0] = 0;

    FILE* cmd = _popen(command, "r");
    if (cmd)
    {
        b32 stay_in_loop = true;
        s32 fetch_counter = 0;
        s32 taken_space = 0;
        while (!feof(cmd) && fetch_counter < 3 && stay_in_loop)
        {
            char temp[128];
            if (fgets(temp, sizeof(temp), cmd) != NULL)
            {
                s32 new_data_length = (s32)strlen(temp);
                s32 space_left = output_size - taken_space;
                if (new_data_length > space_left)
                {
                    stay_in_loop = false;
                    new_data_length = space_left;
                }

                strncat_s(output, output_size, temp, new_data_length);
            }

            fetch_counter += 1;
        }
        _pclose(cmd);
    }
}

s32 get_ping_value(const char *command_result, s32 default_value)
{
    const char time_tag[] = "time=";
    const char ttl_tag[] = "ms TTL";
    const char *time = strstr(command_result, time_tag);
    const char *ttl = strstr(command_result, ttl_tag);
    s32 ping = 0;

    if ((time && ttl) && (ttl > time))
    {
        s32 offset = sizeof(time_tag) - 1;
        time += offset;

        char *test = 0;
        ping = (s32)strtol(time, &test, 10);
    }
    
    if (ping == 0)
    {
        ping = default_value;
    }

    return ping;
}

int main()
{
    #define USER_STRING_SIZE 256
    char domain[USER_STRING_SIZE] = "google.com";
    s32 ping_period = 1000;

    char command[USER_STRING_SIZE];
    snprintf(command, sizeof(command), "ping -n 1 -w %d %s", ping_period, domain); 

    u32 next_time_target = GetTickCount() + ping_period;
    for (;;)
    {
        // pinging
        char command_result[512];
        run_command(command, command_result, sizeof(command_result));
        s32 ping = get_ping_value(command_result, ping_period);

        // printing
        printf("ping: %dms\n", ping);

        // sleeping
        u32 current_time = GetTickCount();
        s32 time_to_sleep = (s32)(next_time_target - current_time);
        if (time_to_sleep < -ping_period)
        {
            time_to_sleep = -ping_period;
        }

        next_time_target = current_time + time_to_sleep + ping_period;

        if (time_to_sleep > 0)
        {
            Sleep(time_to_sleep);
        }
    }
}