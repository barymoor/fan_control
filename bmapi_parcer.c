#include <jansson.h>
#include <stdio.h>
#include <limits.h>
#include "bmapi_parser.h"
#include "logger.h"

int get_max_chip_temperature(const char* estats_reply, int* max_temp)
{
    json_t *root;
    json_error_t error;
    int i;
    
    root = json_loads(estats_reply, 0, &error);
    if(!json_is_object(root))
    {
        app_log(LOG_ERR, "bmapi parser: root is not an object!\n");
        json_decref(root);
        return -1;
    }

    json_t* stats = json_object_get(root, "STATS");
    if(!json_is_array(stats))
    {
        app_log(LOG_ERR, "bmapi parser: stats is not an array!\n");
        json_decref(root);
        return -1;
    }

    stats = json_array_get(stats, 0);
    if(!json_is_object(stats))
    {
        app_log(LOG_ERR, "bmapi parser: stats is not an object!\n");
        json_decref(root);
        return -1;
    }

    *max_temp = INT_MIN;
    for(i = 1; i <= 16; i++)
    {
        char node_id[16];
        sprintf(node_id, "temp2_%d", i);

        json_t *temperature = json_object_get(stats, node_id);
        if(!json_is_integer(temperature))
        {
            app_log(LOG_ERR, "bmapi parser: temperature %s is not an integer\n", node_id);
            json_decref(root);
            return -1;
        }
        int temperature_val = json_integer_value(temperature);
        if (temperature_val > *max_temp)
            *max_temp = temperature_val;

        app_log(LOG_DEBUG, "%s: %d\n", node_id, temperature_val);
    }
    if(*max_temp == INT_MIN)
    {
        app_log(LOG_ERR, "bmapi parser: can't get max temperature. No hash boards?\n");

    }

    json_decref(root);
    return 0;
}
