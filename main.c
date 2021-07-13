#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include "driver-bitmain.h"
#include "logger.h"
#include "query_bmapi.h"
#include "bmapi_parser.h"

#define PWM_LOGGING_PERCENT_STEP 5
#define FAN_STOP_DELAY_TICKS 1200 

int fan_num=0;
unsigned char	fan_exist[BITMAIN_MAX_FAN_NUM]={0};
unsigned int	fan_speed_value[BITMAIN_MAX_FAN_NUM]={0};
unsigned int *axi_fpga_addr = NULL;

int bitmain_axi_init()
{
    unsigned int data;
    int ret=0;

    int device_fd = open("/dev/axi_fpga_dev", O_RDWR);
    if(device_fd < 0)
    {
        app_log(LOG_ERR, "/dev/axi_fpga_dev open failed. fd = %d\n", device_fd);
        perror("open");
        return -1;
    }

    axi_fpga_addr = mmap(NULL, TOTAL_LEN, PROT_READ|PROT_WRITE, MAP_SHARED, device_fd, 0);
    if(!axi_fpga_addr)
    {
        app_log(LOG_ERR, "mmap axi_fpga_addr failed. axi_fpga_addr = 0x%x\n", (unsigned int)axi_fpga_addr);
        return -1;
    }
    app_log(LOG_DEBUG, "mmap axi_fpga_addr = 0x%x\n", (unsigned int)axi_fpga_addr);

    //check the value in address 0xff200000
    data = (*axi_fpga_addr & 0x0000ffff);

    app_log(LOG_DEBUG, "axi_fpga_addr data = 0x%x\n", data);
    return ret;
}

int get_fan_control(void)
{
    int ret = -1;
    ret = *((unsigned int *)(axi_fpga_addr + FAN_CONTROL));
//    app_log(LOG_DEBUG,"%s: FAN_CONTROL is 0x%x\n", __FUNCTION__, ret);
    return ret;
}

void set_fan_control(unsigned int value)
{
    *((unsigned int *)(axi_fpga_addr + FAN_CONTROL)) = value;
//    app_log(LOG_DEBUG,"%s: set FAN_CONTROL is 0x%x\n", __FUNCTION__, value);
    get_fan_control();
}

int get_fan_speed(unsigned char *fan_id, unsigned int *fan_speed)
{
    int ret = -1;
    ret = *((unsigned int *)(axi_fpga_addr + FAN_SPEED));
    *fan_speed = 0x000000ff & ret;
    *fan_id = (unsigned char)(0x00000007 & (ret >> 8));
    // if(*fan_speed > 0)
    // {
    //    app_log(LOG_DEBUG,"%s: fan_id is 0x%x, fan_speed is 0x%x\n", __FUNCTION__, *fan_id, *fan_speed);
    // }
    return ret;
}

void check_fan()
{
    int i=0;
    unsigned char fan_id = 0;
    unsigned int fan_speed;

    for(i=0; i < BITMAIN_MAX_FAN_NUM; i++)
    {
        if(get_fan_speed(&fan_id, &fan_speed) != -1)
        {
            fan_speed_value[fan_id] = fan_speed * 60 * 2;
            if((fan_speed > 0) && (fan_exist[fan_id] == 0))
            {
                fan_exist[fan_id] = 1;
                fan_num++;
                app_log(LOG_DEBUG, "FAN %u found, speed = %u\n", fan_id, fan_speed_value[fan_id]);
            }
            else if((fan_speed == 0) && (fan_exist[fan_id] == 1))
            {
                app_log(LOG_WARNING, "Not found FAN %u! Speed = %u!\n", fan_id, fan_speed_value[fan_id]);
                fan_exist[fan_id] = 0;
                fan_num--;
            }
        }
    }
    app_log(LOG_DEBUG, "Found %d fans\n", fan_num);
}

unsigned int set_PWM(unsigned char pwm_percent)
{
    uint16_t pwm_high_value = 0, pwm_low_value = 0;
    int temp_pwm_percent = 0;
	unsigned int	pwm_value;
	
    temp_pwm_percent = pwm_percent;

    if(temp_pwm_percent < MINIMORUM_PWM_PERCENT)
    {
        temp_pwm_percent = MINIMORUM_PWM_PERCENT;
    }

    if(temp_pwm_percent > MAX_PWM_PERCENT)
    {
        temp_pwm_percent = MAX_PWM_PERCENT;
    }

    pwm_high_value = temp_pwm_percent * PWM_SCALE / 100;
    pwm_low_value  = (100 - temp_pwm_percent) * PWM_SCALE / 100;
    pwm_value = (pwm_high_value << 16) | pwm_low_value;

    set_fan_control(pwm_value);
    return pwm_value;
}

typedef struct {
    unsigned int pwm_value;
    bool is_bmminer_detected; 
    bool is_bmminer_not_detected;
    int bmminer_stopped_tick_counter;
    int last_fan_percent;
    int last_logged_fan_percent;
    struct {
        bool is_started;
        int target_temp;
        int Err;
        int iErr;
        double K;
        double Ti;
        double Td;
    } PID;
} context;

void ctx_init(context* ctx, int target_temp)
{
    ctx->is_bmminer_detected = false;
    ctx->is_bmminer_not_detected = false;
    ctx->bmminer_stopped_tick_counter = 0;
    ctx->last_fan_percent = MINIMORUM_PWM_PERCENT;
    ctx->last_logged_fan_percent = 0;

    ctx->PID.is_started = false;
    ctx->PID.target_temp = target_temp;
    ctx->PID.iErr = 0;
    ctx->PID.K=3.0;
    ctx->PID.Ti=6.0;
    ctx->PID.Td=0.5;
}

void set_fan_percent(context* ctx, int percent)
{
    ctx->last_fan_percent = percent;
    ctx->pwm_value = set_PWM(percent);
}

void stop_fans(context* ctx)
{
    app_log (LOG_INFO, "Stop fans");
    set_fan_percent(ctx, MINIMORUM_PWM_PERCENT);
}

void hold_pwm(context* ctx)
{
    unsigned int cur_pwm = get_fan_control();
    if (ctx->last_fan_percent != MINIMORUM_PWM_PERCENT && cur_pwm != ctx->pwm_value)
    {
        app_log (LOG_DEBUG, "Got pwm=%x, setting pwm to %x\n", cur_pwm, ctx->pwm_value);
        set_fan_control(ctx->pwm_value);
    }
}

int fill_bmminer_presence (context* ctx, bmapi_err_code res)
{
    if (res == BMAPI_FAIL) 
    {
        app_log(LOG_ERR, "Fatal!\n");
        return -1;
    }
    if (res == BMAPI_CANNOT_CONNECT)
    {
        if (ctx->is_bmminer_not_detected)
            return 0;

        app_log(LOG_INFO, "No bmminer. Waiting...\n");
        ///  TODO: perhaps, do quick cooling first
        ctx->bmminer_stopped_tick_counter = FAN_STOP_DELAY_TICKS;
        ctx->is_bmminer_not_detected = true;
        ctx->is_bmminer_detected = false;
        ctx->PID.is_started = false;
        ctx->PID.iErr = 0;
        return 0;
    }

    if (ctx->is_bmminer_detected)
        return 0;

    app_log(LOG_INFO, "Bmminer is appeared to be running.\n");
    ctx->is_bmminer_not_detected = false;
    ctx->is_bmminer_detected = true;
    ctx->bmminer_stopped_tick_counter = 0;
    return 0;
}

int PID_regulate(context* ctx, int temp)
{
    int Err = ctx->PID.target_temp - temp;
    int dErr = ctx->PID.is_started ? Err - ctx->PID.Err : 0;
    int iErr = ctx->PID.iErr + Err;
    int percent = 80 - (int)(ctx->PID.K * (Err + iErr/ctx->PID.Ti + ctx->PID.Td * dErr));
    if (percent < MIN_PWM_PERCENT)
        percent = MIN_PWM_PERCENT;
    if (percent > MAX_PWM_PERCENT)
        percent = MAX_PWM_PERCENT;
    //  Do not integrate if we are out of working range so that accumulated error won't get too high
    if ((percent != MIN_PWM_PERCENT || Err < 0) && (percent != MAX_PWM_PERCENT || Err > 0))
        ctx->PID.iErr = iErr;
    ctx->PID.Err = Err;
    ctx->PID.is_started = true;
    return percent;
}

int set_fan_speed(context* ctx)
{
    char buffer[16384];

    bmapi_err_code res = query_bmapi("{\"command\":\"estats\"}", buffer, sizeof(buffer));
    if (fill_bmminer_presence(ctx, res) != 0)
        return -1;
    
    if (ctx->is_bmminer_not_detected)
        return 0;
        
    int temp;
    if (get_max_chip_temperature(buffer, &temp) != 0)
        return -1;

    int fan_percent = PID_regulate(ctx, temp);
    if (abs(ctx->last_logged_fan_percent - fan_percent) >= PWM_LOGGING_PERCENT_STEP)
    {
        app_log(
            LOG_INFO,
            "Current temperature: %d, iErr=%d, set Fan to %d%%\n",
            temp,
            ctx->PID.iErr,
            fan_percent);
        ctx->last_logged_fan_percent = fan_percent;
    }
    set_fan_percent(ctx, fan_percent);

    return 0;
} 

int run(unsigned int target_temp)
{
    context ctx;
    ctx_init(&ctx, target_temp);
    
    int pwm_hold_interval = 1;  //  100ms
    int pwm_hold_counter = 1;
    int bmminer_check_interval = 100;   //  10s
    int bmminer_check_counter = 1;
    
    for (;;) 
    {
        if (bmminer_check_counter == 1)
        {
            if (set_fan_speed(&ctx) != 0)
                return -1;
            
            bmminer_check_counter = bmminer_check_interval;
        }
        else {
            --bmminer_check_counter;
        }

        if (pwm_hold_counter == 1)
        {
            hold_pwm(&ctx);
            pwm_hold_counter = pwm_hold_interval;  
        }
        else {
            --pwm_hold_counter;
        }

        if (ctx.bmminer_stopped_tick_counter > 0)
        {
            if (--ctx.bmminer_stopped_tick_counter == 0)
                stop_fans(&ctx);
        }
        usleep(100000);
    } 
}

int main(int argc, char* argv[])
{
    int target_temp;

    if (argc != 2 && (argc != 3 || (argc == 3 && strcmp(argv[1], "-d") != 0))) {
        printf ("Usage: %s [-d] <target temperature>\n\t -d - to run in daemon mode\n", argv[0]);        
        return -1;
    }

    openlog("fan_control", LOG_CONS | LOG_PID, LOG_USER);
    if (strcmp(argv[1], "-d") == 0) {
        target_temp = atoi(argv[2]);
        if (daemon(0, 0) != 0)
        {
            syslog(LOG_ERR, "daemon() error!");
            return -1;
        }
        app_log(LOG_INFO, "Started as daemon");
    }
    else {
        target_temp = atoi(argv[1]);
        app_log(LOG_INFO, "Started as process");
    }

    if (bitmain_axi_init()){
        app_log(LOG_ERR, "Can't init axi!\n");
        return -1;
    }

    int res = run(target_temp);
    app_log(LOG_INFO, "Exited");
    closelog();
    return res;
 }
