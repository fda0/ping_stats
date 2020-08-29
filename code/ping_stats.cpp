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


struct History
{
    s32 filled_big;
    s32 filled_small;
    s32 current_index;
    
    s32 big_sum;
    s32 big_avg;
    s32 big_limit;
    
    s32 small_sum;
    s32 small_avg;
    s32 small_limit;
};

internal void
update_history(History *h, s32 *values, s32 big_size, s32 small_size, s32 limit,
               s32 new_value)
{
    if (h->filled_big == big_size)
    {
        s32 last_value = values[h->current_index];
        s32 last_limit = (last_value > limit) ? 1 : 0;
        
        h->big_sum -= last_value;
        h->big_limit -= last_limit;
    }
    else ++h->filled_big;
    
    
    if (h->filled_small == small_size)
    {
        s32 last_index = h->current_index - small_size;
        if (last_index < 0) last_index += big_size;
        
        s32 last_value = values[last_index];
        s32 last_limit = (last_value > limit) ? 1 : 0;
        
        h->small_sum -= last_value;
        h->small_limit -= last_limit;
    }
    else ++h->filled_small;
    
    
    
    values[h->current_index] = new_value;
    
    h->big_sum += new_value;
    h->small_sum += new_value;
    
    s32 new_limit = (new_value > limit) ? 1 : 0;
    h->big_limit += new_limit;
    h->small_limit += new_limit;
    
    h->big_avg   = h->big_sum   / h->filled_big;
    h->small_avg = h->small_sum / h->filled_big;
    
    
    
    if (++h->current_index >= big_size)
    {
        h->current_index = 0;
    }
}

enum Input_Mode
{
    InputMode_None,
    InputMode_Timeout,
    InputMode_Small,
    InputMode_Big,
    InputMode_Limit
};

int main(int argument_count, char **arguments)
{
    char domain[1024] = "google.com";
    s32 ping_period = 1000;
    s32 big_size = 60*60;
    s32 small_size = 60*2;
    s32 limit = 200;
    
    Input_Mode mode = InputMode_None;
    
    for (s32 arg_index = 1;
         arg_index < argument_count;
         ++arg_index)
    {
        char *arg = arguments[arg_index];
        
        if (mode == InputMode_None)
        {
            if (arg[0] == '-' || arg[0] == '/' || arg[0] == '?')
            {
                if (arg[0] == '?' || arg[1] == 'h' || arg[1] == '?')
                {
                    printf("Usage: pint_stats [-w timeout] [-b big_size]\n"
                           "                  [-s small_size] [-l limit]\n"
                           "                  [target_domain_name]\n");
                    exit(0);
                }
                else if (arg[1] == 'w') mode = InputMode_Timeout;
                else if (arg[1] == 'b') mode = InputMode_Big;
                else if (arg[1] == 's') mode = InputMode_Small;
                else if (arg[1] == 'l') mode = InputMode_Limit;
                else 
                {
                    printf("unknown switch, use -h, ? or /? for help\n");
                    exit(0);
                }
            }
            else
            {
                strncpy(domain, arg, Array_Count(domain) - 1);
                domain[Array_Count(domain) - 1] = 0;
            }
        }
        else if (mode == InputMode_Timeout ||
                 mode == InputMode_Big ||
                 mode == InputMode_Small ||
                 mode == InputMode_Limit)
        {
            s32 value = atoi(arg);
            if (value <= 0)
            {
                printf("incorrect numerical value: %s\n", arg);
            }
            else
            {
                if (mode == InputMode_Timeout) ping_period = value;
                else if (mode == InputMode_Big) big_size = value;
                else if (mode == InputMode_Small) small_size = value;
                else if (mode == InputMode_Limit) limit = value;
            }
            
            mode = InputMode_None;
        }
    }
    
    small_size = Minimum(small_size, big_size);
    s32 *ping_array = (s32 *)malloc(big_size*sizeof(s32));
    History history = {};
    
    printf("Starting with settings: small_size: %d, big_size %d, limit: %d\n"
           "domain: %s\n",
           small_size, big_size, limit, domain);
    
    
    char command[2048];
    snprintf(command, sizeof(command), "ping -n 1 -w %d %s", ping_period, domain); 
    
    
    
    u32 next_time_target = GetTickCount() + ping_period;
    for (;;)
    {
        // pinging
        char command_result[512];
        run_command(command, command_result, sizeof(command_result));
        s32 ping = get_ping_value(command_result, ping_period);
        update_history(&history, ping_array, big_size, small_size, limit, ping);
        
        // printing
        printf("ping: %dms\ts_avg: %dms\ts_lim: %d\t\tb_avg: %dms\tb_lim: %d\n", ping, history.small_avg, history.small_limit, history.big_avg, history.big_limit);
        
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