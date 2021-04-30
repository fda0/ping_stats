#include <windows.h>
#include <stdio.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef s32 b32;
typedef float f32;
typedef double f64;
#define Array_Count(Array) (sizeof(Array) / sizeof(*(Array)))
#define Minimum(A, B) (A > B ? B : A)

enum Color
{
    Color_Normal,
    Color_Red,
    Color_Yellow,
    Color_Green,
    Color_LightRed,
    Color_LightYellow,
    Color_LightGreen,
    Color_BackgroudRed,
    Color_BackgroudYellow,
    Color_Default, // gray reset
    
    Color_Count
};

struct Color_Value
{
    Color color;
    char *text;
};

//~ NOTE: Globals
static Color_Value color_values[Color_Count] = {
    {Color_Normal, "\033[39m"},
    {Color_Red, "\033[31m"},
    {Color_Yellow, "\033[33m"},
    {Color_Green, "\033[32m"},
    {Color_LightRed, "\033[91m"},
    {Color_LightYellow, "\033[93m"},
    {Color_LightGreen, "\033[92m"},
    {Color_BackgroudRed, "\033[39m\033[41m"},
    {Color_BackgroudYellow, "\033[39m\033[43m"},
    {Color_Default, "\033[90m\033[49m"},
};

static b32 are_colors_enabled = false;

#define Help_Text \
"ping_stats [-w timeout] [-l limit_threshold]\n"\
"           [-s small_buffer_record_count] [-b big_buffer_record_count]\n"\
"           [target_address]\n"




///////////////////////////////
inline char *
get_color_text(Color color)
{
    char *result = "";
    if (are_colors_enabled && color < Array_Count(color_values)) {
        result = color_values[color].text;
    }
    return result;
};


static char *
get_color(f32 value, s32 limit)
{
    f32 limit_100 = (f32)limit;
    f32 limit_75 = 0.75f*limit_100;
    f32 limit_50 = 0.5f*limit_100;
    f32 limit_25 = 0.25f*limit_100;
    
    char *result = "";
    
    if (value > limit_100) {
        result = get_color_text(Color_BackgroudRed);
    } else if (value > limit_75) {
        result = get_color_text(Color_Red);
    } else if (value > limit_50) {
        result = get_color_text(Color_LightRed);
    } else if (value > limit_25) {
        result = get_color_text(Color_Yellow);
    } else {
        result = get_color_text(Color_LightGreen);
    }
    
    return result;
}





static void 
run_command(const char* command, char *output, s32 output_size)
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


static s32 
get_ping_value(char *command_result, s32 default_value)
{
    s32 ping = default_value;
    
    // NOTE: Process MS Ping output
    char time_tag[] = "time=";
    char ttl_tag[] = "ms";
    
    char *time = strstr(command_result, time_tag);
    if (time)
    {
        char *ttl = strstr(command_result, ttl_tag);
        if (ttl)
        {
            s32 offset = sizeof(time_tag) - 1;
            time += offset;
            
            ping = (s32)strtol(time, NULL, 10);
        }
    }
    
    return ping;
}


struct History
{
    s32 filled_big;
    s32 filled_small;
    s32 current_index;
    
    s32 big_sum;
    s32 big_limit;
    f32 big_avg;
    
    s32 small_sum;
    s32 small_limit;
    f32 small_avg;
};

static void
update_history(History *h, s32 *values, s32 big_size, s32 small_size, s32 limit, s32 new_value)
{
    // NOTE: Decrement big range sum and limit coutners once array starts wrapping
    if (h->filled_big == big_size)
    {
        s32 last_value = values[h->current_index];
        s32 last_limit = (last_value > limit) ? 1 : 0;
        
        h->big_sum -= last_value;
        h->big_limit -= last_limit;
    }
    else ++h->filled_big;
    
    // NOTE: Decrement small range sum and limit coutners once grows to small_size
    if (h->filled_small == small_size)
    {
        s32 last_index = h->current_index - small_size;
        if (last_index < 0) last_index += big_size;
        
        s32 last_value = values[last_index];
        s32 last_limit = (last_value > limit) ? 1 : 0;
        
        h->small_sum -= last_value;
        h->small_limit -= last_limit;
    }
    else
    {
        ++h->filled_small;
    }
    
    
    // NOTE: Add new ping value to sums and calculate if it is above limit.
    values[h->current_index] = new_value;
    
    h->big_sum += new_value;
    h->small_sum += new_value;
    
    s32 new_limit = (new_value > limit) ? 1 : 0;
    h->big_limit += new_limit;
    h->small_limit += new_limit;
    
    h->big_avg   = (f32)h->big_sum   / (f32)h->filled_big;
    h->small_avg = (f32)h->small_sum / (f32)h->filled_small;
    
    
    
    if (++h->current_index >= big_size) {
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
    {
        // NOTE: Windows cmd color mode init.
        HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
        if (handle != INVALID_HANDLE_VALUE)
        {
            DWORD dwMode = 0;
            if (GetConsoleMode(handle, &dwMode))
            {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                if (SetConsoleMode(handle, dwMode))
                {
                    are_colors_enabled = true;
                }    
            }
        }
    }
    
    
    char domain[1024] = "google.com";
    s32 ping_timeout = 1000;
    s32 big_size = 60*60;
    s32 small_size = 60*2;
    s32 limit = 200;
    
    Input_Mode mode = InputMode_None;
    
    // NOTE: Command line arguments handling
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
                    printf(Help_Text);
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
            s32 value = (s32)strtol(arg, NULL, 10);
            if (value <= 0)
            {
                printf("incorrect numerical value: %s\n", arg);
                exit(0);
            }
            else
            {
                if (mode == InputMode_Timeout) ping_timeout = value;
                else if (mode == InputMode_Big) big_size = value;
                else if (mode == InputMode_Small) small_size = value;
                else if (mode == InputMode_Limit) limit = value;
            }
            
            mode = InputMode_None;
        }
    }
    
    
    
    // NOTE: Prepare inputs and allocate memory
    small_size = Minimum(small_size, big_size);
    limit = Minimum(ping_timeout - 1, limit);
    s32 *ping_array = (s32 *)malloc(big_size*sizeof(s32));
    History history = {};
    
    printf(Help_Text);
    printf("Settings:\n"
           "target address: %s%s%s\n"
           "timeout: %s%d%s\n"
           "small buffer will hold %s%d%s records\n"
           "big buffer will hold %s%d%s record\n"
           "avg - average of the buffer\n"
           "lim - number of records in buffer with value above %s%d%s\n"
           "%s===============================================================================\n%s",
           get_color_text(Color_LightYellow), domain, get_color_text(Color_Normal),
           get_color_text(Color_LightYellow), ping_timeout, get_color_text(Color_Normal),
           get_color_text(Color_LightGreen), small_size,  get_color_text(Color_Normal),
           get_color_text(Color_LightGreen), big_size, get_color_text(Color_Normal),
           get_color_text(Color_LightRed), limit, get_color_text(Color_Normal),
           get_color_text(Color_Default), get_color_text(Color_Normal));
    
    char command[2048];
    snprintf(command, sizeof(command), "ping -n 1 -w %d %s", ping_timeout, domain); 
    
    
    
    u32 next_time_target = GetTickCount() + ping_timeout;
    for (;;)
    {
        // NOTE: Do the pinging and process its output
        char command_result[1024];
        run_command(command, command_result, sizeof(command_result));
        s32 ping = get_ping_value(command_result, ping_timeout);
        update_history(&history, ping_array, big_size, small_size, limit, ping);
        
        
        // NOTE: Calculate colors and print the output
        printf("%s"
               "ping: %s%4dms"
               "%s    "
               "small: {"
               "avg: %s%3.0fms"
               "%s; "
               "lim: %s%3d"
               "%s}    "
               "big: {"
               "avg: %s%3.0fms"
               "%s; "
               "lim: %s%3d"
               "%s}\n",
               get_color_text(Color_Normal),
               get_color((f32)ping, limit), ping,
               get_color_text(Color_Default),
               get_color(history.small_avg, limit), history.small_avg,
               get_color_text(Color_Default),
               get_color((f32)history.small_limit, small_size/8), history.small_limit,
               get_color_text(Color_Default),
               get_color(history.big_avg, limit), history.big_avg,
               get_color_text(Color_Default),
               get_color((f32)history.big_limit, big_size/16), history.big_limit,
               get_color_text(Color_Default));
        
        
        
        // NOTE: Calculate sleep time and sleep until next ping.
        u32 current_time = GetTickCount();
        s32 time_to_sleep = (s32)(next_time_target - current_time);
        if (time_to_sleep < -ping_timeout)
        {
            time_to_sleep = -ping_timeout;
        }
        
        next_time_target = current_time + time_to_sleep + ping_timeout;
        
        if (time_to_sleep > 0)
        {
            Sleep(time_to_sleep);
        }
    }
}