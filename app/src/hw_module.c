#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/rtc.h>

#include <stdio.h>

#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>

#include <zephyr/drivers/sensor/npm1300_charger.h>
#include <zephyr/dt-bindings/regulator/npm1300.h>
#include <zephyr/drivers/mfd/npm1300.h>

#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/zbus/zbus.h>

#include <time.h>
#include <zephyr/posix/time.h>

#include <nrfx_clock.h>
#include <nrfx_spim.h>

#include "max30001.h"
#include "max32664.h"

#include "bmi323_hpi.h"

#ifdef CONFIG_SENSOR_MAX32664C
#include "max32664c.h"
#endif

#include "hw_module.h"
#include "fs_module.h"
#include "ui/move_ui.h"

// #include "max32664c_msbl.h"

#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/mfd/npm1300.h>
#include <zephyr/drivers/regulator.h>

#include <zephyr/pm/device.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device_runtime.h>

#include "nrf_fuel_gauge.h"

LOG_MODULE_REGISTER(hw_module);
char curr_string[40];

/*******EXTERNS******/
extern struct k_msgq q_session_cmd_msg;

ZBUS_CHAN_DECLARE(sys_time_chan, batt_chan);
ZBUS_CHAN_DECLARE(steps_chan);
ZBUS_CHAN_DECLARE(temp_chan);

/****END EXTERNS****/

// Peripheral Device Pointers
static const struct device *max30208_dev = DEVICE_DT_GET_ANY(maxim_max30208);
static const struct device *max32664d_dev = DEVICE_DT_GET_ANY(maxim_max32664);
static const struct device *max32664c_dev = DEVICE_DT_GET_ANY(maxim_max32664c);

const struct device *imu_dev = DEVICE_DT_GET(DT_NODELABEL(bmi323));
const struct device *const max30001_dev = DEVICE_DT_GET(DT_ALIAS(max30001));
static const struct device *rtc_dev = DEVICE_DT_GET(DT_ALIAS(rtc));
const struct device *usb_cdc_uart_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);
const struct device *const gpio_keys_dev = DEVICE_DT_GET(DT_NODELABEL(gpiokeys));
const struct device *const w25_flash_dev = DEVICE_DT_GET(DT_NODELABEL(w25q01jv));

// PMIC Device Pointers
static const struct device *regulators = DEVICE_DT_GET(DT_NODELABEL(npm_pmic_regulators));
static const struct device *ldsw_disp_unit = DEVICE_DT_GET(DT_NODELABEL(npm_pmic_ldo1));
static const struct device *ldsw_sens_1_8 = DEVICE_DT_GET(DT_NODELABEL(npm_pmic_ldo2));
static const struct device *charger = DEVICE_DT_GET(DT_NODELABEL(npm_pmic_charger));
static const struct device *pmic = DEVICE_DT_GET(DT_NODELABEL(npm_pmic));

const struct device *display_dev = DEVICE_DT_GET(DT_NODELABEL(sh8601)); // DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
const struct device *touch_dev = DEVICE_DT_GET_ONE(chipsemi_chsc5816);

// LED Power DC/DC Enable
static const struct gpio_dt_spec dcdc_5v_en = GPIO_DT_SPEC_GET(DT_NODELABEL(sensor_dcdc_en), gpios);

volatile bool max30001_device_present = false;
volatile bool max32664c_device_present = false;
volatile bool max32664d_device_present = false;

static volatile bool vbus_connected;

// static const struct device npm_gpio_keys = DEVICE_DT_GET(DT_NODELABEL(npm_pmic_buttons));
// static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET(DT_ALIAS(gpio_button0), gpios);

static uint8_t global_batt_level = 0;
// int32_t global_temp;
// bool global_batt_charging = false;
static struct rtc_time global_system_time;

uint8_t hw_second_boot __attribute__((section(".noinit")));

// USB CDC UART
#define RING_BUF_SIZE 1024
uint8_t ring_buffer[RING_BUF_SIZE];
struct ring_buf ringbuf_usb_cdc;
static bool rx_throttled;

K_SEM_DEFINE(sem_hw_inited, 0, 1);
K_SEM_DEFINE(sem_start_cal, 0, 1);

K_SEM_DEFINE(sem_ecg_intb_recd, 0, 1);
K_SEM_DEFINE(sem_ppg_finger_thread_start, 0, 1);
K_SEM_DEFINE(sem_ecg_bioz_thread_start, 0, 1);
K_SEM_DEFINE(sem_ppg_sm_start, 0, 1);
K_SEM_DEFINE(sem_display_on, 0, 1);
K_SEM_DEFINE(sem_disp_boot_complete, 0, 1);

K_SEM_DEFINE(sem_hw_thread_start, 0, 1);

K_SEM_DEFINE(sem_crown_key_pressed, 0, 1);

#define MOVE_SAMPLING_DISABLED 0

static float max_charge_current;
static float term_charge_current;
static int64_t ref_time;

void ecg_sampling_trigger(void);

static const struct battery_model battery_model = {
#include "battery_profile_200.inc"
};

/* nPM1300 CHARGER.BCHGCHARGESTATUS.CONSTANTCURRENT register bitmask */
#define NPM1300_CHG_STATUS_CC_MASK BIT_MASK(3)

static void gpio_keys_cb_handler(struct input_event *evt)
{
    // printk("GPIO_KEY %s pressed, zephyr_code=%u, value=%d type=%d\n",
    //        evt->dev->name, evt->code, evt->value, evt->type);

    if (evt->value == 1)
    {
        switch (evt->code)
        {
        case INPUT_KEY_UP:
            LOG_INF("Crown Key Pressed");
            k_sem_give(&sem_crown_key_pressed);
            break;
        case INPUT_KEY_HOME:
            LOG_INF("Extra Key Pressed");
            sys_reboot(SYS_REBOOT_COLD);
            // printk("Entering Ship Mode\n");
            // regulator_parent_ship_mode(regulators);
            break;
        default:
            break;
        }
    }
}

void send_usb_cdc(const char *buf, size_t len)
{
    int rb_len;
    rb_len = ring_buf_put(&ringbuf_usb_cdc, buf, len);
    uart_irq_tx_enable(usb_cdc_uart_dev);
}

static void usb_cdc_uart_interrupt_handler(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    while (uart_irq_update(dev) && uart_irq_is_pending(dev))
    {
        if (!rx_throttled && uart_irq_rx_ready(dev))
        {
            int recv_len, rb_len;
            uint8_t buffer[64];
            size_t len = MIN(ring_buf_space_get(&ringbuf_usb_cdc),
                             sizeof(buffer));

            if (len == 0)
            {

                uart_irq_rx_disable(dev);
                rx_throttled = true;
                continue;
            }

            recv_len = uart_fifo_read(dev, buffer, len);
            if (recv_len < 0)
            {
                LOG_ERR("Failed to read UART FIFO");
                recv_len = 0;
            };

            rb_len = ring_buf_put(&ringbuf_usb_cdc, buffer, recv_len);
            if (rb_len < recv_len)
            {
                LOG_ERR("Drop %u bytes", recv_len - rb_len);
            }

            LOG_DBG("tty fifo -> ringbuf %d bytes", rb_len);
            if (rb_len)
            {
                uart_irq_tx_enable(dev);
            }
        }

        if (uart_irq_tx_ready(dev))
        {
            uint8_t buffer[64];
            int rb_len, send_len;

            rb_len = ring_buf_get(&ringbuf_usb_cdc, buffer, sizeof(buffer));
            if (!rb_len)
            {
                LOG_DBG("Ring buffer empty, disable TX IRQ");
                uart_irq_tx_disable(dev);
                continue;
            }

            if (rx_throttled)
            {
                uart_irq_rx_enable(dev);
                rx_throttled = false;
            }

            send_len = uart_fifo_fill(dev, buffer, rb_len);
            if (send_len < rb_len)
            {
                LOG_ERR("Drop %d bytes", rb_len - send_len);
            }

            LOG_DBG("ringbuf -> tty fifo %d bytes", send_len);
        }
    }
}

uint8_t read_battery_level(void)
{
    uint8_t batt_level = 0;

    /*int ret = 0;

    fuel_gauge_prop_t props[] = {
        FUEL_GAUGE_RUNTIME_TO_EMPTY,
        FUEL_GAUGE_RUNTIME_TO_FULL,
        FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE,
        FUEL_GAUGE_VOLTAGE,
    };

    union fuel_gauge_prop_val vals[ARRAY_SIZE(props)];

    ret = fuel_gauge_get_props(fg_dev, props, vals, ARRAY_SIZE(props));
    if (ret < 0)
    {
        printk("Error: cannot get properties\n");
    }
    else
    {
        // printk("Time to empty %d\n", vals[0].runtime_to_empty);
        // printk("Time to full %d\n", vals[1].runtime_to_full);
        // printk("Charge %d%%\n", vals[2].relative_state_of_charge);
        // printk("Voltage %d\n", vals[3].voltage);

        batt_level = vals[2].relative_state_of_charge;
    }*/

    return batt_level;
}

static int npm_read_sensors(const struct device *charger,
                            float *voltage, float *current, float *temp, int32_t *chg_status)
{
    struct sensor_value value;
    int ret;

    ret = sensor_sample_fetch(charger);
    if (ret < 0)
    {
        return ret;
    }

    sensor_channel_get(charger, SENSOR_CHAN_GAUGE_VOLTAGE, &value);
    *voltage = (float)value.val1 + ((float)value.val2 / 1000000);

    sensor_channel_get(charger, SENSOR_CHAN_GAUGE_TEMP, &value);
    *temp = (float)value.val1 + ((float)value.val2 / 1000000);

    sensor_channel_get(charger, SENSOR_CHAN_GAUGE_AVG_CURRENT, &value);
    *current = (float)value.val1 + ((float)value.val2 / 1000000);

    sensor_channel_get(charger, SENSOR_CHAN_NPM1300_CHARGER_STATUS, &value);
    *chg_status = value.val1;

    return 0;
}

int npm_fuel_gauge_init(const struct device *charger)
{
    struct sensor_value value;
    struct nrf_fuel_gauge_init_parameters parameters = {
        .model = &battery_model,
        .opt_params = NULL,
    };
    int32_t chg_status;
    int ret;

    LOG_DBG("nRF Fuel Gauge version: %s", nrf_fuel_gauge_version);

    ret = npm_read_sensors(charger, &parameters.v0, &parameters.i0, &parameters.t0, &chg_status);
    if (ret < 0)
    {
        return ret;
    }

    /* Store charge nominal and termination current, needed for ttf calculation */
    sensor_channel_get(charger, SENSOR_CHAN_GAUGE_DESIRED_CHARGING_CURRENT, &value);
    max_charge_current = (float)value.val1 + ((float)value.val2 / 1000000);
    term_charge_current = max_charge_current / 10.f;

    nrf_fuel_gauge_init(&parameters, NULL);

    ref_time = k_uptime_get();

    return 0;
}

int npm_fuel_gauge_update(const struct device *charger, bool vbus_connected)
{
    float voltage;
    float current;
    float temp;
    float soc;
    float tte;
    float ttf;
    float delta;
    int32_t chg_status;
    bool cc_charging;
    int ret;

    ret = npm_read_sensors(charger, &voltage, &current, &temp, &chg_status);
    if (ret < 0)
    {
        printk("Error: Could not read from charger device\n");
        return ret;
    }

    cc_charging = (chg_status & NPM1300_CHG_STATUS_CC_MASK) != 0;

    delta = (float)k_uptime_delta(&ref_time) / 1000.f;

    soc = nrf_fuel_gauge_process(voltage, current, temp, delta, vbus_connected, NULL);
    tte = nrf_fuel_gauge_tte_get();
    ttf = nrf_fuel_gauge_ttf_get(cc_charging, -term_charge_current);

    // printk("V: %.3f, I: %.3f, T: %.2f, ", voltage, current, temp);
    // printk("SoC: %.2f, TTE: %.0f, TTF: %.0f, ", soc, tte, ttf);
    // printk("Charge status: %d\n", chg_status);
    struct batt_status batt_s = {
        .batt_level = (uint8_t)soc,
        .batt_charging = (chg_status & NPM1300_CHG_STATUS_CC_MASK) != 0,
    };

    zbus_chan_pub(&batt_chan, &batt_s, K_SECONDS(1));
    hw_set_battery_level((uint8_t)soc);

    return 0;
}

static int usb_init()
{
    int ret = 0;

    ret = usb_enable(NULL);
    if (ret != 0)
    {
        LOG_ERR("Failed to enable USB");
        // return;
    }

    /* Enabled USB CDC interrupts */

    ring_buf_init(&ringbuf_usb_cdc, sizeof(ring_buffer), ring_buffer);

    uint32_t dtr = 0U;

    uart_line_ctrl_get(usb_cdc_uart_dev, UART_LINE_CTRL_DTR, &dtr);

    k_msleep(100);
    uart_irq_callback_set(usb_cdc_uart_dev, usb_cdc_uart_interrupt_handler);
    uart_irq_rx_enable(usb_cdc_uart_dev);

    printk("\nUSB Init complete\n\n");

    return ret;
}

double read_temp_f(void)
{
    struct sensor_value temp_sample;
   
    sensor_sample_fetch(max30208_dev);
    sensor_channel_get(max30208_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp_sample);
    // last_read_temp_value = temp_sample.val1;
    double temp_c = (double)temp_sample.val1 * 0.005;
    double temp_f = (temp_c * 1.8) + 32.0;
    //printk("Temp: %.2f F\n", temp_f);
    return temp_f;
}

void hw_rtc_set_device_time(uint8_t m_sec, uint8_t m_min, uint8_t m_hour, uint8_t m_day, uint8_t m_month, uint8_t m_year)
{
    struct rtc_time time_set;

    time_set.tm_sec = m_sec;
    time_set.tm_min = m_min;
    time_set.tm_hour = m_hour;
    time_set.tm_mday = m_day;
    time_set.tm_mon = m_month;
    time_set.tm_year = m_year;

    int ret = rtc_set_time(rtc_dev, &time_set);
    printk("RTC Set Time: %d\n", ret);
}

#define DEFAULT_DATE 240428 // YYMMDD 28th April 2024
#define DEFAULT_TIME 121212 // HHMMSS 12:12:12

#define DEFAULT_FUTURE_DATE 240429 // YYMMDD 29th April 2024
#define DEFAULT_FUTURE_TIME 121213 // HHMMSS 12:12:13

void hw_bpt_start_cal(void)
{
    LOG_INF("Starting BPT Calibration\n");

    struct sensor_value data_time_val;
    data_time_val.val1 = DEFAULT_DATE; // Date
    data_time_val.val2 = DEFAULT_TIME; // Time
    sensor_attr_set(max32664d_dev, SENSOR_CHAN_ALL, MAX32664_ATTR_DATE_TIME, &data_time_val);
    k_sleep(K_MSEC(100));

    struct sensor_value bp_cal_val;
    bp_cal_val.val1 = 0x00787A7D; // Sys vals
    bp_cal_val.val2 = 0x00505152; // Dia vals
    sensor_attr_set(max32664d_dev, SENSOR_CHAN_ALL, MAX32664_ATTR_BP_CAL, &bp_cal_val);
    k_sleep(K_MSEC(100));

    // Start BPT Calibration
    struct sensor_value mode_val;
    mode_val.val1 = MAX32664_OP_MODE_BPT_CAL_START;
    sensor_attr_set(max32664d_dev, SENSOR_CHAN_ALL, MAX32664_ATTR_OP_MODE, &mode_val);
    k_sleep(K_MSEC(100));

    // ppg_data_start();
}

void hw_bpt_start_est(void)
{
    struct sensor_value load_cal;
    load_cal.val1 = 0x00000000;
    // sensor_attr_set(max32664_dev, SENSOR_CHAN_ALL, MAX32664_ATTR_LOAD_CALIB, &load_cal);

    struct sensor_value data_time_val;
    data_time_val.val1 = DEFAULT_FUTURE_DATE; // Date // TODO: Update to local time
    data_time_val.val2 = DEFAULT_FUTURE_TIME; // Time
    sensor_attr_set(max32664d_dev, SENSOR_CHAN_ALL, MAX32664_ATTR_DATE_TIME, &data_time_val);
    k_sleep(K_MSEC(100));

    struct sensor_value mode_set;
    mode_set.val1 = MAX32664_OP_MODE_BPT;
    sensor_attr_set(max32664d_dev, SENSOR_CHAN_ALL, MAX32664_ATTR_OP_MODE, &mode_set);

    k_sleep(K_MSEC(1000));
    // ppg_data_start();
}

void hw_bpt_stop(void)
{
    struct sensor_value mode_val;
    mode_val.val1 = MAX32664_ATTR_STOP_EST;
    sensor_attr_set(max32664d_dev, SENSOR_CHAN_ALL, MAX32664_ATTR_STOP_EST, &mode_val);
}

void hw_bpt_get_calib(void)
{
    struct sensor_value mode_val;
    mode_val.val1 = MAX32664_OP_MODE_BPT_CAL_GET_VECTOR;
    sensor_attr_set(max32664d_dev, SENSOR_CHAN_ALL, MAX32664_ATTR_OP_MODE, &mode_val);
}

void hw_pwr_display_enable(void)
{
    regulator_enable(ldsw_disp_unit);
}

K_MUTEX_DEFINE(mutex_batt_level);

void hw_set_battery_level(uint8_t batt_level)
{
    k_mutex_lock(&mutex_batt_level, K_FOREVER);
    global_batt_level = batt_level;
    k_mutex_unlock(&mutex_batt_level);
}

uint8_t hw_get_battery_level(void)
{
    return global_batt_level;
}

static void trigger_handler(const struct device *dev, const struct sensor_trigger *trigger)
{
    ARG_UNUSED(trigger);

    if (sensor_sample_fetch(dev))
    {
        printf("Acc Trig\n");
        return;
    }

    // k_sem_give(&sem);
}

bool hw_is_max32664c_present(void)
{
    return max32664c_device_present;
}

int hw_max32664c_set_op_mode(uint8_t op_mode, uint8_t algo_mode)
{
    struct sensor_value mode_set;
    mode_set.val1 = op_mode;
    mode_set.val2 = algo_mode;
    return sensor_attr_set(max32664c_dev, SENSOR_CHAN_ALL, MAX32664C_ATTR_OP_MODE, &mode_set);
}

int hw_max30001_ecg_enable(bool enable)
{
    struct sensor_value ecg_mode_set;
    if (enable == true)
    {
        ecg_mode_set.val1 = 1;
    }
    else
    {
        ecg_mode_set.val1 = 0;
    }

    sensor_attr_set(max30001_dev, SENSOR_CHAN_ALL, MAX30001_ATTR_ECG_ENABLED, &ecg_mode_set);
}

int hw_max30001_bioz_enable(bool enable)
{
    struct sensor_value bioz_mode_set;
    if (enable == true)
    {
        bioz_mode_set.val1 = 1;
    }
    else
    {
        bioz_mode_set.val1 = 0;
    }
    return sensor_attr_set(max30001_dev, SENSOR_CHAN_ALL, MAX30001_ATTR_BIOZ_ENABLED, &bioz_mode_set);
}

static void event_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (pins & BIT(NPM1300_EVENT_VBUS_DETECTED))
    {
        printk("Vbus connected\n");
        vbus_connected = true;
    }

    if (pins & BIT(NPM1300_EVENT_VBUS_REMOVED))
    {
        printk("Vbus removed\n");
        vbus_connected = false;
    }
}

void hw_init(void)
{
    int ret = 0;
    static struct rtc_time curr_time;

    struct sensor_trigger imu_trig = {
        .type = SENSOR_TRIG_DATA_READY,
        .chan = SENSOR_CHAN_ACCEL_XYZ,
    };

    k_sem_give(&sem_display_on);

    if (!device_is_ready(pmic))
    {
        LOG_ERR("Pmic device not ready");
        return 0;
    }

    if (!device_is_ready(regulators))
    {
        LOG_ERR("Error: Regulator device is not ready\n");
        // return 0;
    }

    if (!device_is_ready(charger))
    {
        LOG_ERR("Charger device not ready.\n");
        // return 0;
    }
    if (npm_fuel_gauge_init(charger) < 0)
    {
        LOG_ERR("Could not initialise fuel gauge.\n");
        // return 0;
    }

    static struct gpio_callback event_cb;

    gpio_init_callback(&event_cb, event_callback,
                       BIT(NPM1300_EVENT_VBUS_DETECTED) |
                           BIT(NPM1300_EVENT_VBUS_REMOVED));

    ret = mfd_npm1300_add_callback(pmic, &event_cb);
    if (ret)
    {
        LOG_ERR("Failed to add pmic callback");
        return 0;
    }

    /* Initialise vbus detection status. */
    struct sensor_value val;
    ret = sensor_attr_get(charger, SENSOR_CHAN_CURRENT, SENSOR_ATTR_UPPER_THRESH, &val);

    if (ret < 0)
    {
        return false;
    }

    vbus_connected = (val.val1 != 0) || (val.val2 != 0);

    if (!device_is_ready(imu_dev))
    {
        LOG_ERR("Error: Accelerometer device not ready");
    }
    else
    {
        if (sensor_trigger_set(imu_dev, &imu_trig, trigger_handler) < 0)
        {
            LOG_ERR("Could not set trigger");
            // return 0;
        }
        else
        {
            LOG_INF("IMU Trigger set");
            struct sensor_value set_val;
            set_val.val1 = 1;
            sensor_attr_set(imu_dev, SENSOR_CHAN_ACCEL_XYZ, BMI323_HPI_ATTR_EN_FEATURE_ENGINE, &set_val);
            sensor_attr_set(imu_dev, SENSOR_CHAN_ACCEL_XYZ, BMI323_HPI_ATTR_EN_STEP_COUNTER, &set_val);
        }
    }

    // device_init(display_dev);
    //  k_sleep(K_MSEC(1000));
    // hw_pwr_display_enable();

    // regulator_enable(ldsw_sens_1_8);
    regulator_disable(ldsw_sens_1_8);

    k_sleep(K_MSEC(100));

    // device_init(display_dev);
    // k_sleep(K_MSEC(100));
    device_init(touch_dev);

    //printk("hw_second_boot: %d\n", hw_second_boot);
    
    /*if(hw_second_boot!=1)
    {
        hw_second_boot = 1;
        sys_reboot(SYS_REBOOT_WARM);
    }*/

    if (!device_is_ready(max30001_dev))
    {
        LOG_ERR("MAX30001 device not found!");
        max30001_device_present = false;
    }
    else
    {
        LOG_INF("MAX30001 device found!");
        max30001_device_present = true;

        // Disable ECG and BIOZ by default
        hw_max30001_ecg_enable(false);
        hw_max30001_bioz_enable(false);
    }

    k_sleep(K_MSEC(1000));

    ret = gpio_pin_configure_dt(&dcdc_5v_en, GPIO_OUTPUT_ACTIVE);
    if (ret < 0)
    {
        LOG_ERR("Error: Could not configure GPIO pin DC/DC 5v EN\n");
    }

    gpio_pin_set_dt(&dcdc_5v_en, 1);
    k_sleep(K_MSEC(1000));

    if (!device_is_ready(max32664c_dev))
    {
        LOG_ERR("MAX32664C device not present!");
        max32664c_device_present = false;
    }
    else
    {
        LOG_INF("MAX32664C device present!");
        max32664c_device_present = true;

        struct sensor_value mode_get;
        sensor_attr_get(max32664c_dev, SENSOR_CHAN_ALL, MAX32664C_ATTR_IS_APP_PRESENT, &mode_get);

        LOG_INF("MAX32664C App Present: %d", mode_get.val1);
        
        // To force bootloader mode
        //mode_get.val1 = 8;

        if (mode_get.val1 == 8)
        {
            LOG_INF("MAX32664C App not present. Starting bootloader mode");
            struct sensor_value mode_set;
            mode_set.val1 = 1;
            sensor_attr_set(max32664c_dev, SENSOR_CHAN_ALL, MAX32664C_ATTR_ENTER_BOOTLOADER, &mode_set);
        }
        else
        {
            LOG_INF("MAX32664C App present");

            // Moved all start commands to SMF PPG
        }
    }

    k_sleep(K_MSEC(1000));

    if (!device_is_ready(max32664d_dev))
    {
        LOG_ERR("MAX32664D device not present!");
        max32664d_device_present = false;
    }
    else
    {
        LOG_INF("MAX32664D device present!");
        max32664d_device_present = true;

        struct sensor_value mode_set;

        // Set initial mode
        mode_set.val1 = MAX32664_OP_MODE_BPT;
        sensor_attr_set(max32664d_dev, SENSOR_CHAN_ALL, MAX32664_ATTR_OP_MODE, &mode_set);
    }

#ifdef NRF_SPIM_HAS_32_MHZ_FREQ
    LOG_INF("SPIM runs at 32MHz !");
#endif

    // setup_pmic_callbacks();

    if (!device_is_ready(max30208_dev))
    {
        LOG_ERR("MAX30208 device not found!");
    }
    else
    {
        LOG_INF("MAX30208 device found!");
    }


    rtc_get_time(rtc_dev, &curr_time);
    LOG_INF("RTC time: %d:%d:%d %d/%d/%d", curr_time.tm_hour, curr_time.tm_min, curr_time.tm_sec, curr_time.tm_mon, curr_time.tm_mday, curr_time.tm_year);

    npm_fuel_gauge_update(charger, vbus_connected);
    
    fs_module_init();

    pm_device_runtime_get(gpio_keys_dev);

    INPUT_CALLBACK_DEFINE(gpio_keys_dev, gpio_keys_cb_handler, NULL);

    // pm_device_runtime_put(w25_flash_dev);

    k_sem_give(&sem_hw_inited);

    // Start sampling only if devices are present

    if (max30001_device_present)
    {
        k_sem_give(&sem_ecg_bioz_thread_start);
    }

    if (max32664c_device_present)
    {

        k_sem_give(&sem_ppg_sm_start);
    }

    if (max32664d_device_present)
    {
        k_sem_give(&sem_ppg_finger_thread_start);
    }

    k_sem_give(&sem_disp_boot_complete);
    k_sem_give(&sem_hw_thread_start);

    LOG_INF("HW Init complete");

    // init_settings();

    // usb_init();
}

struct rtc_time hw_get_current_time(void)
{
    rtc_get_time(rtc_dev, &global_system_time);
    return global_system_time;
}

static uint32_t acc_get_steps(void)
{
    struct sensor_value steps;
    sensor_sample_fetch(imu_dev);
    sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_X, &steps);
    return (uint32_t)steps.val1;
}

void hw_thread(void)
{
    struct rtc_time sys_time;
    uint32_t _steps = 0;
    double _temp_f = 0.0;

    k_sem_take(&sem_hw_thread_start, K_FOREVER);
    LOG_INF("HW Thread starting");

    for (;;)
    {
        npm_fuel_gauge_update(charger, vbus_connected);

        rtc_get_time(rtc_dev, &sys_time);
        zbus_chan_pub(&sys_time_chan, &sys_time, K_SECONDS(1));

        _temp_f = read_temp_f();
        _steps = acc_get_steps();

        struct hpi_steps_t steps = {
            .steps_walk = _steps,
        };
        zbus_chan_pub(&steps_chan, &steps, K_SECONDS(1));

        struct hpi_temp_t temp = {
            .temp_f = _temp_f,
        };
        zbus_chan_pub(&temp_chan, &temp, K_SECONDS(1));

        k_sleep(K_MSEC(5000));
    }
}

#define HW_THREAD_STACKSIZE 4096
#define HW_THREAD_PRIORITY 7

K_THREAD_DEFINE(hw_thread_id, HW_THREAD_STACKSIZE, hw_thread, NULL, NULL, NULL, HW_THREAD_PRIORITY, 0, 0);
