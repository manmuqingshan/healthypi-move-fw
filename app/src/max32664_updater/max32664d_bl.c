/*
 * HealthyPi Move
 * 
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Protocentral Electronics
 *
 * Author: Ashwin Whitchurch, Protocentral Electronics
 * Contact: ashwin@protocentral.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#define DT_DRV_COMPAT maxim_max32664

#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/drivers/gpio.h>
#include "max32664d.h"

//extern const uint8_t *max32664d_msbl;
//#include "max32664d_msbl.h"
extern const uint8_t *max32664d_msbl;

LOG_MODULE_REGISTER(MAX32664_BL, CONFIG_MAX32664D_LOG_LEVEL);

uint8_t max32664_fw_init_vector[11] = {0};
uint8_t max32664_fw_auth_vector[16] = {0};

#define MAX32664_FW_UPDATE_WRITE_SIZE 8208 // Page size 8192 + 16 bytes for CRC
#define MAX32664_FW_UPDATE_START_ADDR 0x4C

#define MAX32664D_FW_BIN_INCLUDE 0

static int m_read_bl_ver(const struct device *dev)
{
	const struct max32664d_config *config = dev->config;
	uint8_t rd_buf[4] = {0x00, 0x00, 0x00, 0x00};
	uint8_t wr_buf[2] = {0x81, 0x00};

	k_sleep(K_USEC(300));
	i2c_write_dt(&config->i2c, wr_buf, sizeof(wr_buf));
	k_sleep(K_MSEC(MAX32664_DEFAULT_CMD_DELAY));

	i2c_read_dt(&config->i2c, rd_buf, sizeof(rd_buf));
	k_sleep(K_MSEC(MAX32664_DEFAULT_CMD_DELAY));

	LOG_DBG("BL Version = %d.%d.%d\n", rd_buf[1], rd_buf[2], rd_buf[3]);

	return 0;
}

static int m_read_op_mode(const struct device *dev)
{ 
    const struct max32664d_config *config = dev->config;
    uint8_t rd_buf[2] = {0x00, 0x00};

    uint8_t wr_buf[2] = {0x02, 0x00};

    k_sleep(K_USEC(300));
    i2c_write_dt(&config->i2c, wr_buf, sizeof(wr_buf));
    k_sleep(K_MSEC(45));
    gpio_pin_set_dt(&config->mfio_gpio, 0);
    k_sleep(K_USEC(300));
    i2c_read_dt(&config->i2c, rd_buf, sizeof(rd_buf));
    k_sleep(K_MSEC(45));
    gpio_pin_set_dt(&config->mfio_gpio, 1);

    // LOG_INF("Op mode = %x\n", rd_buf[1]);

    return rd_buf[1];
}

static int m_read_bl_page_size(const struct device *dev, uint16_t *bl_page_size)
{
	const struct max32664d_config *config = dev->config;
	uint8_t rd_buf[2] = {0x00, 0x00};
	uint8_t wr_buf[2] = {0x81, 0x01};

	k_sleep(K_USEC(300));
	i2c_write_dt(&config->i2c, wr_buf, sizeof(wr_buf));
	k_sleep(K_MSEC(MAX32664_DEFAULT_CMD_DELAY));

	i2c_read_dt(&config->i2c, rd_buf, sizeof(rd_buf));
	k_sleep(K_MSEC(MAX32664_DEFAULT_CMD_DELAY));

	// printk("BL PS Read: %x %x\n", rd_buf[0], rd_buf[1]);
	*bl_page_size = (uint16_t)(rd_buf[1] << 8) | rd_buf[2];

	return 0;
}

static int m_write_set_num_pages(const struct device *dev, uint8_t num_pages)
{
	const struct max32664d_config *config = dev->config;
	uint8_t wr_buf[4] = {0x80, 0x02, 0x00, 0x00};
	uint8_t rd_buf[1] = {0x00};

	wr_buf[3] = num_pages;

	k_sleep(K_USEC(300));
	i2c_write_dt(&config->i2c, wr_buf, sizeof(wr_buf));
	k_sleep(K_MSEC(MAX32664_DEFAULT_CMD_DELAY));

	i2c_read_dt(&config->i2c, rd_buf, sizeof(rd_buf));
	k_sleep(K_MSEC(MAX32664_DEFAULT_CMD_DELAY));

	printk("Write Num Pages RSP: %x %x\n", rd_buf[0]);
}

static int m_write_init_vector(const struct device *dev, uint8_t *init_vector)
{
	const struct max32664d_config *config = dev->config;
	uint8_t wr_buf[13];
	uint8_t rd_buf[1] = {0x00};

	wr_buf[0] = 0x80;
	wr_buf[1] = 0x00;

	memcpy(&wr_buf[2], init_vector, 11);

	k_sleep(K_USEC(300));
	i2c_write_dt(&config->i2c, wr_buf, sizeof(wr_buf));
	k_sleep(K_MSEC(MAX32664_DEFAULT_CMD_DELAY));

	i2c_read_dt(&config->i2c, rd_buf, sizeof(rd_buf));
	k_sleep(K_MSEC(MAX32664_DEFAULT_CMD_DELAY));

	LOG_DBG("Write Init Vec RSP: %x", rd_buf[0]);
}

static int m_write_auth_vector(const struct device *dev, uint8_t *auth_vector)
{
	const struct max32664d_config *config = dev->config;
	uint8_t wr_buf[18];
	uint8_t rd_buf[1] = {0x00};

	wr_buf[0] = 0x80;
	wr_buf[1] = 0x01;

	memcpy(&wr_buf[2], auth_vector, 16);

	k_sleep(K_USEC(300));
	i2c_write_dt(&config->i2c, wr_buf, sizeof(wr_buf));
	k_sleep(K_MSEC(MAX32664_DEFAULT_CMD_DELAY));

	i2c_read_dt(&config->i2c, rd_buf, sizeof(rd_buf));
	k_sleep(K_MSEC(MAX32664_DEFAULT_CMD_DELAY));

	LOG_DBG("Write Auth Vec : RSP: %x", rd_buf[0]);
}

//volatile uint8_t fw_data_wr_buf[MAX32664_FW_UPDATE_WRITE_SIZE + 2];

// Reduced from 8x1026 (8.2KB) to single buffer approach
static uint8_t tmp_wr_buf[1026]; // Single buffer, reused for each message

static int m_fw_write_page(const struct device *dev, uint8_t *msbl_data, uint32_t msbl_page_offset)
{
	const struct max32664d_config *config = dev->config;

	uint8_t rd_buf[1] = {0x00};
	uint8_t cmd_wr_buf[2] = {0x80, 0x04};

	// memcpy(&fw_data_wr_buf[2], &max32664d_msbl[msbl_page_offset], (MAX32664_FW_UPDATE_WRITE_SIZE+2));
	// memcpy(fw_data_wr_buf, &maxm86146_msbl[msbl_page_offset], MAX32664_FW_UPDATE_WRITE_SIZE);

	int msg_len = 1026;
	int num_msgs = ((MAX32664_FW_UPDATE_WRITE_SIZE) / msg_len);

	struct i2c_msg max32664_i2c_msg[num_msgs];

	LOG_DBG("Num Msgs: %d", num_msgs);

	// Memory-optimized approach: Send messages one at a time instead of parallel
	// First send the command
	i2c_write_dt(&config->i2c, cmd_wr_buf, sizeof(cmd_wr_buf));
	k_sleep(K_MSEC(MAX32664_DEFAULT_CMD_DELAY));

#if (MAX32664D_FW_BIN_INCLUDE == 1)
	// Send data messages sequentially to reduce memory usage
	for (int i = 0; i < num_msgs; i++)
	{
		memcpy(tmp_wr_buf, &max32664d_msbl[(i * msg_len) + msbl_page_offset], msg_len);
		
		int ret = i2c_write_dt(&config->i2c, tmp_wr_buf, msg_len);
		if (ret != 0) {
			LOG_ERR("Failed to write message %d, error: %d", i, ret);
			return ret;
		}
		k_sleep(K_USEC(100)); // Small delay between messages
		LOG_DBG("Sent msg %d of %d", i + 1, num_msgs);
#else
	LOG_WRN("Firmware binary not included, skipping data send");
#endif

	// Read response
	k_sleep(K_MSEC(2000));
	i2c_read_dt(&config->i2c, rd_buf, sizeof(rd_buf));
	k_sleep(K_MSEC(MAX32664_DEFAULT_CMD_DELAY));

	LOG_DBG("Write Page RSP: %x", rd_buf[0]);

	return 0;
}

static int m_erase_app(const struct device *dev)
{
	const struct max32664d_config *config = dev->config;
	uint8_t wr_buf[2] = {0x80, 0x03};
	uint8_t rd_buf[1] = {0x00};

	k_sleep(K_USEC(300));
	i2c_write_dt(&config->i2c, wr_buf, sizeof(wr_buf));
	k_sleep(K_MSEC(2000));

	i2c_read_dt(&config->i2c, rd_buf, sizeof(rd_buf));
	k_sleep(K_MSEC(MAX32664_DEFAULT_CMD_DELAY));

	LOG_DBG("Erase App : RSP: %x", rd_buf[0]);
}

static int m_read_mcu_id(const struct device *dev)
{
	const struct max32664d_config *config = dev->config;
	uint8_t rd_buf[2] = {0x00, 0x00};
	uint8_t wr_buf[2] = {0xFF, 0x00};

	k_sleep(K_USEC(300));
	i2c_write_dt(&config->i2c, wr_buf, sizeof(wr_buf));
	k_sleep(K_MSEC(MAX32664_DEFAULT_CMD_DELAY));

	i2c_read_dt(&config->i2c, rd_buf, sizeof(rd_buf));
	k_sleep(K_MSEC(MAX32664_DEFAULT_CMD_DELAY));

	printk("MCU ID = %x %x\n", rd_buf[0], rd_buf[1]);

	return 0;
}


static int max32664d_load_fw(const struct device *dev, uint8_t *fw_bin_array)
{
	uint8_t msbl_num_pages = 0;
	uint16_t msbl_write_pos = 0;

#if (MAX32664D_FW_BIN_INCLUDE == 1)
	printk("---\nLoading MSBL\n");
	printk("MSBL Array Size: %d\n", sizeof(max32664d_msbl));

	msbl_num_pages = fw_bin_array[0x44];
	printk("MSBL Load: Pages: %d (%x)\n", msbl_num_pages, msbl_num_pages);

	m_read_mcu_id(dev);

	m_write_set_num_pages(dev, msbl_num_pages);

	memcpy(max32664_fw_init_vector, &fw_bin_array[0x28], 11);
	m_write_init_vector(dev, max32664_fw_init_vector);
	printk("MSBL Init Vector: %x %x %x %x %x %x %x %x %x %x %x\n", max32664_fw_init_vector[0], max32664_fw_init_vector[1], max32664_fw_init_vector[2], max32664_fw_init_vector[3], max32664_fw_init_vector[4], max32664_fw_init_vector[5], max32664_fw_init_vector[6], max32664_fw_init_vector[7], max32664_fw_init_vector[8], max32664_fw_init_vector[9], max32664_fw_init_vector[10]);

	memcpy(max32664_fw_auth_vector, &fw_bin_array[0x34], 16);
	m_write_auth_vector(dev, max32664_fw_auth_vector);

	m_erase_app(dev);
	k_sleep(K_MSEC(2000));

	// Write MSBL

	for (int i = 0; i < msbl_num_pages; i++)
	{
		printk("Writing Page: %d of %d\n", (i + 1), msbl_num_pages);

		// memcpy(max32664_fw_page_buf, &fw_bin_array[MAX32664_FW_UPDATE_START_ADDR + (i * MAX32664_FW_UPDATE_WRITE_SIZE)], MAX32664_FW_UPDATE_WRITE_SIZE);
		uint32_t msbl_page_offset = (MAX32664_FW_UPDATE_START_ADDR + (i * MAX32664_FW_UPDATE_WRITE_SIZE));
		printk("MSBL Page Offset: %d (%x)\n", msbl_page_offset, msbl_page_offset);
		m_fw_write_page(dev, max32664d_msbl, msbl_page_offset);

		k_sleep(K_MSEC(500));
	}
#endif

	max32664d_do_enter_app(dev);

	printk("End Load MSBL\n---\n");

	return 0;
}


void max32664_do_enter_bl(const struct device *dev)
{
	const struct max32664d_config *config = dev->config;

	LOG_INF("Entering Bootloader mode\n");

	gpio_pin_configure_dt(&config->mfio_gpio, GPIO_OUTPUT);

	// Enter BOOTLOADER mode
	gpio_pin_set_dt(&config->mfio_gpio, 0);
	k_sleep(K_MSEC(10));

	gpio_pin_set_dt(&config->reset_gpio, 0);
	k_sleep(K_MSEC(10));

	gpio_pin_set_dt(&config->reset_gpio, 1);
	k_sleep(K_MSEC(1000));
	// End of BOOTLOADER mode

	gpio_pin_configure_dt(&config->mfio_gpio, GPIO_INPUT);

	m_read_op_mode(dev);

	m_read_bl_ver(dev);

	uint16_t bl_page_size;
	m_read_bl_page_size(dev, &bl_page_size);
	LOG_DBG("BL Page Size: %d", bl_page_size);

#if (MAX32664D_FW_BIN_INCLUDE == 1)
	max32664d_load_fw(dev, max32664d_msbl);
#endif
}
