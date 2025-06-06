/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Filename: itecomdbgr.c         For Chipset: ITE EC
 *
 * Function: ITE COM DBGR Flash Utility
 */

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <getopt.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#define ITE_ERR 0xF0

#define FW_UPDATE_START 0x00000
#define FW_UPDATE_END 0x80000

#define NO_READ 0xFFFFFFFF

#define SUCCESS 0
#define FAIL 1

#define msleep(msecs) usleep(msecs * 1000)

#define W_CMD_PORT 0xB4
#define W_DATA_PORT 0x6A
#define R_CMD_PORT 0xB4
#define R_DATA_PORT 0x6B
#define W_BURST_DATA_PORT 0xF2
#define R_BURST_DATA_PORT 0xF3

#define CHIPID_1 0x00
#define CHIPID_2 0x01
#define CHIPIDVER 0x02
#define DBUS_ADDR_0 0x04
#define DBUS_ADDR_1 0x05
#define DBUS_ADDR_2 0x06
#define DBUS_ADDR_3 0x07
#define DBUS_DATA 0x08
#define DBUS_256R_DATA 0x09
#define DBUS_256W_DATA 0x0A
#define EMU_KSI 0x20
#define RAM_ADDR_0 0x2E
#define RAM_ADDR_1 0x2F
#define RAM_DATA 0x30

#define REG_WRITE 0
#define REG_READ 1

/* SPI Command Set*/
/* SPI Page Program command*/
#define SPI_PP 0x02
/* SPI Write Disable command*/
#define SPI_WRDI 0x04
/* SPI Read Status command*/
#define SPI_RDSR 0x05
/* SPI Write Enable command*/
#define SPI_WREN 0x06
/* SPI Fast Read command*/
#define SPI_FAST_READ 0x0B
/* SPI Sector Erase command*/
#define SPI_SE_4K 0x20
#define SPI_SE_1K 0xD7
/* SPI Read ID command*/
#define SPI_RDID 0x9F

/* Retries to set SPI SR1 WEL */
#define SPI_SR1_WEL_RETRIES 10
/* Bits in SPI SR1 */
#define SPI_SR1_BUSY 0x01 /* also WIP */
#define SPI_SR1_WEL 0x02

#define STEPS_EXIT 0x00
#define STEPS_NORMAL 0x01
#define STEPS_TEST 0xEE

const static uint8_t enable_follow_mode[16] = {
	W_CMD_PORT, DBUS_ADDR_3, W_DATA_PORT, 0x7F,
	W_CMD_PORT, DBUS_ADDR_2, W_DATA_PORT, 0xFF,
	W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFF,
	W_CMD_PORT, DBUS_ADDR_0, W_DATA_PORT, 0xFF
};

const static uint8_t disable_follow_mode[8] = { W_CMD_PORT,  DBUS_ADDR_3,
						W_DATA_PORT, 0x40,
						W_CMD_PORT,  DBUS_ADDR_2,
						W_DATA_PORT, 0x00 };

const static uint8_t cs_low[4] = { W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFD };

const static uint8_t cs_high[8] = { W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFE,
				    W_CMD_PORT, DBUS_DATA,   W_DATA_PORT, 0x00 };

const static uint8_t spi_write_enable[16] = {
	W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFD,
	W_CMD_PORT, DBUS_DATA,	 W_DATA_PORT, SPI_WREN,
	W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFE,
	W_CMD_PORT, DBUS_DATA,	 W_DATA_PORT, 0x00
};

/* Config mostly comes from the command line.  Defaults are set in main(). */
struct itecomdbgr_config {
	int g_steps;
	FILE *fi;
	int g_flash_size;
	int g_blk_size;
	int g_blk_no;
	int page_size;
	int sector_size;

	unsigned long int baudrate;
	unsigned long update_start_addr;
	unsigned long update_end_addr;
	unsigned long read_start_addr;
	unsigned long read_range;
	bool noverify;
	char *device_name;
	char *file_name;
	char *read_file_name;
	int file_size;
	int g_fd;
	int eflash_size_in_k;
	uint8_t eflash_type;
	uint8_t sector_erase_pages;
	uint8_t spi_cmd_sector_erase;
	uint8_t G_DBG_BUF[256];
	uint8_t *g_readbuf;
	uint8_t *g_writebuf;
};

#define EFLASH_TYPE_8315 0x01
#define EFLASH_TYPE_KGD 0x02
#define EFLASH_TYPE_NONE 0xFF

#define SPI_CMD_SECTOR_ERASE_1K 0xD7
#define SPI_CMD_SECTOR_ERASE_4K 0x20

const static uint8_t read_id_buf[8] = { W_CMD_PORT,	   DBUS_DATA,
					W_DATA_PORT,	   SPI_RDID,
					W_CMD_PORT,	   DBUS_256R_DATA,
					R_BURST_DATA_PORT, 0x02 };

const static uint8_t read_status_buf[7] = { W_CMD_PORT, DBUS_DATA,  W_DATA_PORT,
					    SPI_RDSR,	W_CMD_PORT, DBUS_DATA,
					    R_DATA_PORT };

static struct termios tty_saved;
volatile static int tty_saved_fd = -1;

static void hexdump(uint8_t *buffer, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if ((i % 16 == 0)) {
			printf(" %06X :", i);
		}
		printf(" %02x", buffer[i]);
		if ((i % 16 == 7)) {
			printf(" - ");
		}
		if (i % 16 == 15) {
			printf("\n");
		}
	}
}

static int init_file(struct itecomdbgr_config *conf)
{
	int r = 0;
	int bytes;
	struct stat st;

	if (conf->read_start_addr != NO_READ)
		return 0;

	conf->fi = fopen(conf->file_name, "rb");
	if (conf->fi != NULL) {
		if (fstat(fileno(conf->fi), &st) < 0) {
			printf("fstat error\n");
			return 1;
		}
		conf->file_size = st.st_size;
		conf->g_writebuf = (uint8_t *)malloc(conf->file_size);
		if (conf->g_writebuf == NULL) {
			printf("alloc g_writebuf fail\n");
		}
		conf->g_readbuf = (uint8_t *)malloc(conf->file_size);
		if (conf->g_readbuf == NULL) {
			printf("alloc g_readbuf fail\n");
		}
		bytes = fread(conf->g_writebuf, 1, conf->file_size, conf->fi);

		if (bytes != conf->file_size) {
			printf("File read only returned %d bytes, %d bytes expected\n",
			       bytes, conf->file_size);
			r = ITE_ERR;
		}
	} else {
		printf("open file error : %s\n", conf->file_name);
		r = ITE_ERR;
	}

	return r;
}

static void exit_file(struct itecomdbgr_config *conf)
{
	free(conf->g_writebuf);
	free(conf->g_readbuf);
	if (conf->read_start_addr == NO_READ)
		fclose(conf->fi);
}

static ssize_t read_com(struct itecomdbgr_config *conf, uint8_t *const inbuff,
			const size_t ReadBytes)
{
	size_t input_cc;
	ssize_t cc;

	for (input_cc = 0; input_cc < ReadBytes; input_cc += cc) {
		cc = read(conf->g_fd, inbuff + input_cc, ReadBytes - input_cc);
		if (cc < 0)
			return -1;
		if (cc == 0)
			break;
	}

	return input_cc;
}

static ssize_t write_com(struct itecomdbgr_config *conf,
			 const uint8_t *lpOutBuffer, int WriteBytes)
{
	ssize_t bWriteStat;

	bWriteStat = write(conf->g_fd, lpOutBuffer, WriteBytes);

	return bWriteStat;
}

/*
 * Discards (flushes) any data received via UART, but not yet retrieved through
 * `read_com()`.
 */
static void flush_com(struct itecomdbgr_config *conf)
{
	fd_set read_fds;
	struct timeval timeout;
	char buf[256];
	int cc;

	/* First ask the kernel to drop any data. */
	tcflush(conf->g_fd, TCIOFLUSH);

	/*
	 * For devices where the above is not properly implemented, we
	 * additionally attempt to manually drain any buffered data below, by
	 * repeatedly reading and discarding, for as long as more data remains
	 * available.  We could have used a zero timeout, but instead chose a
	 * very short timeout of 1ms, just to be sure that the kernel actually
	 * queries the USB device, rather than maybe instantly replying if no
	 * data is buffered in the kernel driver.
	 */
	for (;;) {
		FD_ZERO(&read_fds);
		FD_SET(conf->g_fd, &read_fds);
		timeout.tv_sec = 0;
		timeout.tv_usec = 1000;
		/*
		 * Ask the operating system to wait up to 1ms for data to become
		 * available to read from the serial port.
		 */
		cc = select(conf->g_fd + 1, &read_fds, NULL, NULL, &timeout);
		if (cc < 0)
			fprintf(stderr, "Error from select(): %s\n",
				strerror(errno));
		if (!cc || !FD_ISSET(0, &read_fds)) {
			/* No more data immediately available */
			break;
		}
		/*
		 * Select indicated that data is available to read, get whatever
		 * we can, discard it, and then go back and ask if there is
		 * more.
		 */
		cc = read(conf->g_fd, buf, sizeof(buf));
		if (cc < 0)
			fprintf(stderr, "Error reading serial data: %s\n",
				strerror(errno));
	}
}

static uint8_t debug_getc(struct itecomdbgr_config *conf)
{
	uint8_t data[1];
	ssize_t res;

	res = read(conf->g_fd, data, 1);

	if (res > 0) {
		return data[0];
	} else {
		return 0xFF;
	}
}

static int rw_reg(struct itecomdbgr_config *conf, unsigned long Address,
		  uint8_t RW)
{
	ssize_t cc;

	/* [3][7][11] mapping to Address A2 A1 A0 */
	uint8_t rw_reg_buf[15] = { W_CMD_PORT,
				   0x80,
				   W_DATA_PORT,
				   (uint8_t)(Address >> 16),
				   W_CMD_PORT,
				   0x2F,
				   W_DATA_PORT,
				   (uint8_t)(Address >> 8),
				   W_CMD_PORT,
				   0x2E,
				   W_DATA_PORT,
				   (uint8_t)(Address),
				   W_CMD_PORT,
				   0x30,
				   0 };

	if (RW == REG_WRITE) {
		rw_reg_buf[14] = W_DATA_PORT;
	} else {
		rw_reg_buf[14] = R_DATA_PORT;
	}
	cc = write_com(conf, rw_reg_buf, sizeof(rw_reg_buf));
	if (cc != sizeof(rw_reg_buf))
		return FAIL;

	return SUCCESS;
}

static int wr_reg(struct itecomdbgr_config *conf, unsigned long Address,
		  uint8_t WrD0)
{
	uint8_t Data[1];
	ssize_t cc;

	Data[0] = WrD0;
	if (rw_reg(conf, Address, REG_WRITE) != SUCCESS)
		return FAIL;
	cc = write_com(conf, Data, sizeof(Data));
	if (cc != sizeof(Data))
		return FAIL;

	return SUCCESS;
}

static uint8_t rd_reg_or_ff(struct itecomdbgr_config *conf,
			    unsigned long Address)
{
	if (rw_reg(conf, Address, REG_READ) != SUCCESS)
		return 0xff;

	msleep(1);
	return debug_getc(conf);
}

/* disable protect path from DBGR */
static int dbgr_disable_protect_path(struct itecomdbgr_config *conf)
{
	int ret = 0, i;

	printf("Disabling protect path...\n");

	for (i = 0; i < 32; i++) {
		wr_reg(conf, 0xF02060 + i, 0);
		wr_reg(conf, 0xF020A0 + i, 0);
	}

	if (ret < 0)
		fprintf(stderr, "DISABLE PROTECT PATH FROM DBGR FAILED!\n");

	return ret;
}

static void
enter_uart_dbgr_mode_and_set_nack_mode(struct itecomdbgr_config *conf)
{
	uint8_t Data[1];

	Data[0] = 0x00;
	write_com(conf, Data, sizeof(Data));
	msleep(1);

	Data[0] = W_CMD_PORT;
	write_com(conf, Data, sizeof(Data));
	msleep(1);

	Data[0] = 0x00;
	write_com(conf, Data, sizeof(Data));
	msleep(1);

	Data[0] = W_DATA_PORT;
	write_com(conf, Data, sizeof(Data));
	msleep(1);

	Data[0] = 0x00;
	write_com(conf, Data, sizeof(Data));
	msleep(1);

	Data[0] = W_BURST_DATA_PORT;
	write_com(conf, Data, sizeof(Data));
	msleep(1);

	Data[0] = 0x00;
	write_com(conf, Data, sizeof(Data));

	msleep(5);

	Data[0] = W_CMD_PORT;
	write_com(conf, Data, sizeof(Data));
	Data[0] = 0x80;
	write_com(conf, Data, sizeof(Data));
	msleep(1);

	Data[0] = W_DATA_PORT;
	write_com(conf, Data, sizeof(Data));
	Data[0] = 0xF0;
	write_com(conf, Data, sizeof(Data));
	msleep(1);

	Data[0] = W_CMD_PORT;
	write_com(conf, Data, sizeof(Data));
	Data[0] = 0x2F;
	write_com(conf, Data, sizeof(Data));
	msleep(1);

	Data[0] = W_DATA_PORT;
	write_com(conf, Data, sizeof(Data));
	Data[0] = 0x40;
	write_com(conf, Data, sizeof(Data));
	msleep(1);

	Data[0] = W_CMD_PORT;
	write_com(conf, Data, sizeof(Data));
	Data[0] = 0x2E;
	write_com(conf, Data, sizeof(Data));
	msleep(1);

	Data[0] = W_DATA_PORT;
	write_com(conf, Data, sizeof(Data));
	Data[0] = 0x00;
	write_com(conf, Data, sizeof(Data));
	msleep(1);

	Data[0] = W_CMD_PORT;
	write_com(conf, Data, sizeof(Data));
	Data[0] = 0x30;
	write_com(conf, Data, sizeof(Data));
	msleep(1);

	Data[0] = W_DATA_PORT;
	write_com(conf, Data, sizeof(Data));
	Data[0] = 0x03;
	write_com(conf, Data, sizeof(Data));
	msleep(1);
}

/* Return 0 on success, non-zero on a timeout */
static int check_status(struct itecomdbgr_config *conf, uint8_t wait_mask,
			uint8_t f)
{
	uint8_t status;
	int timeout = 0;
	uint8_t check_value = wait_mask;

	if (f)
		check_value = 0;

	do {
		write_com(conf, cs_low, sizeof(cs_low));
		write_com(conf, read_status_buf, sizeof(read_status_buf));
		status = debug_getc(conf);
		write_com(conf, cs_high, sizeof(cs_high));
		if (timeout++ > 200) {
			printf("check_status timeout exit!\n");
			return -1;
		}

		/* Add a delay for 3M speed*/
		if (conf->baudrate == 3000000)
			msleep(1);

	} while ((status & wait_mask) == check_value);

	return 0;
}

static int getchipid(struct itecomdbgr_config *conf)
{
	uint8_t chipid[3], chipver, eflash_size_flag;

	uint8_t test[3] = { W_CMD_PORT, 0x00, R_DATA_PORT };

	/* Get CHIPID_1 for dbgr command set */
	write_com(conf, test, 3);
	chipid[1] = debug_getc(conf); /* read for clear buffer */

	chipid[0] = rd_reg_or_ff(conf, 0xF02085);
	chipid[1] = rd_reg_or_ff(conf, 0xF02086);
	chipid[2] = rd_reg_or_ff(conf, 0xF02087);
	chipver = rd_reg_or_ff(conf, 0xF02002);

	if ((chipid[0] != 0x08) && (chipid[0] != 0x05)) {
		/* Get Chip ID Fail */
		return FAIL;
	}

	printf("Chip ID = %02x %02x %02x", chipid[0], chipid[1], chipid[2]);
	printf(", Chip Ver = %02x", chipver);

	eflash_size_flag = chipver >> 4;
	if (eflash_size_flag == 0xC)
		conf->eflash_size_in_k = 1024;
	if (eflash_size_flag == 0x8)
		conf->eflash_size_in_k = 512;
	printf(", eflash size = %4d KB", conf->eflash_size_in_k);
	printf(", file size = %4d KB\n", conf->file_size / 1024);

	/* Get the real flash size , 64K for 1 Block*/
	/* Reset the global flash value */
	conf->g_flash_size = conf->eflash_size_in_k * 1024;
	conf->g_blk_size = conf->eflash_size_in_k / 64;

	if (conf->read_range == 0)
		conf->read_range = conf->g_flash_size;

	conf->update_start_addr = 0;
	if (conf->file_size < conf->g_flash_size) {
		conf->update_end_addr = conf->file_size;
	} else {
		conf->update_end_addr = conf->g_flash_size;
	}
	return SUCCESS;
}

static int read_id_2(struct itecomdbgr_config *conf)
{
	int result;
	uint8_t FlashID[3];

	write_com(conf, enable_follow_mode, sizeof(enable_follow_mode));
	write_com(conf, cs_low, sizeof(cs_low));
	write_com(conf, read_id_buf, sizeof(read_id_buf));

	FlashID[0] = debug_getc(conf);
	FlashID[1] = debug_getc(conf);
	FlashID[2] = debug_getc(conf);

	write_com(conf, cs_high, sizeof(cs_high));
	write_com(conf, disable_follow_mode, sizeof(disable_follow_mode));
	printf("Flash ID = %02x %02x %02x\n", FlashID[0], FlashID[1],
	       FlashID[2]);
	flush_com(conf);

	if ((FlashID[0] == 0xFF) && (FlashID[1] == 0xFF) &&
	    (FlashID[2] == 0xFE)) {
		printf("FLASH TYPE = 8315\n");
		conf->eflash_type = EFLASH_TYPE_8315;
		result = SUCCESS;
	} else if ((FlashID[0] == 0xC8) || (FlashID[0] == 0xEF)) {
		printf("FLASH TYPE = KGD\n");
		conf->eflash_type = EFLASH_TYPE_KGD;
		result = SUCCESS;
		conf->g_steps = STEPS_EXIT;
	} else {
		printf("Invalid EFLASH TYPE\n");
		conf->eflash_type = EFLASH_TYPE_NONE;
		result = FAIL;
	}
	return result;
}

static int spi_sr1_wel(struct itecomdbgr_config *conf)
{
	for (int i = 0; i < SPI_SR1_WEL_RETRIES; ++i) {
		write_com(conf, spi_write_enable, sizeof(spi_write_enable));
		if (check_status(conf, SPI_SR1_WEL, 1) == 0) {
			if (i > 0) {
				printf("%s: SUCCESS on try %d\n", __func__,
				       i + 1);
			}
			return SUCCESS;
		}
	}

	return FAIL;
}

static void print_delta(const char *msg, int *prev_percent, int new_percent)
{
	if (new_percent != *prev_percent) {
		printf("\r%-17s: %3d%%", msg, new_percent);
		*prev_percent = new_percent;
		fflush(stdout);
	}
}

static int erase_flash(struct itecomdbgr_config *conf)
{
	int i = 0;
	int result = SUCCESS;
	unsigned long start_addr = conf->update_start_addr;
	unsigned long end_addr = conf->update_end_addr;
	int total_sectors = (end_addr - start_addr) / conf->sector_size;
	int prev_percent = -1;
	int progress_percent;

	/* [3] mapping to spi erase command ,*/
	/* [7][11][15] mapping to Address A2 A1 A0 */
	uint8_t erase_buf[16] = {
		W_CMD_PORT, DBUS_DATA, W_DATA_PORT, SPI_SE_4K,
		W_CMD_PORT, DBUS_DATA, W_DATA_PORT, 0x00,
		W_CMD_PORT, DBUS_DATA, W_DATA_PORT, 0x00,
		W_CMD_PORT, DBUS_DATA, W_DATA_PORT, 0x00,
	};

	write_com(conf, enable_follow_mode, sizeof(enable_follow_mode));
	while (start_addr < end_addr) {
		if (spi_sr1_wel(conf) != SUCCESS) {
			printf("erase_4k:check_status error 1\n");
			result = FAIL;
			goto out;
		}
		write_com(conf, cs_low, sizeof(cs_low));
		erase_buf[7] = start_addr >> 16;
		erase_buf[11] = start_addr >> 8;
		erase_buf[15] = 0;
		write_com(conf, erase_buf, sizeof(erase_buf));
		write_com(conf, cs_high, sizeof(cs_high));
		if (check_status(conf, SPI_SR1_BUSY, 0) < 0) {
			printf("%s: SPI_SR1_BUSY\n", __func__);
			result = FAIL;
			goto out;
		}
		start_addr += conf->sector_size;

		progress_percent = (++i * 100) / total_sectors;
		print_delta("Erasing...", &prev_percent, progress_percent);
	}
out:
	write_com(conf, disable_follow_mode, sizeof(disable_follow_mode));
	if (result == SUCCESS)
		printf("\n");
	return result;
}

static uint8_t fastread_buf[24] = {
	W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       SPI_FAST_READ,
	W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
	W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
	W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
	W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
	W_CMD_PORT, DBUS_256R_DATA, R_BURST_DATA_PORT, 0xFF
};

static int fast_read_burst_cdata(struct itecomdbgr_config *conf,
				 uint8_t *C_Data, bool check_erased)
{
	unsigned int i = 0;
	uint8_t DBG_BUF[256];
	uint8_t allff[256];
	int j = 0;
	int read_count = 0;
	int count;
	int result = SUCCESS;
	unsigned long start_addr = conf->update_start_addr;
	unsigned long end_addr = conf->update_end_addr;
	FILE *pW = NULL;

	int total_size = (end_addr - start_addr) / conf->page_size;

	if ((end_addr - start_addr) % conf->page_size)
		total_size += 1;

	if (conf->read_start_addr != NO_READ) {
		pW = fopen(conf->read_file_name, "w");
		fseek(pW, 0, SEEK_SET);
	}

	if (check_erased) {
		for (i = 0; i < 256; i++) {
			allff[i] = 0xFF;
			DBG_BUF[i] = i;
		}
	}

	write_com(conf, enable_follow_mode, sizeof(enable_follow_mode));

	int prev_percent = -1;
	int progress_percent;

	while (start_addr < end_addr) {
		ssize_t cc;

		if ((end_addr - start_addr) >= conf->page_size)
			read_count = conf->page_size;
		else
			read_count = end_addr - start_addr;

		if (check_status(conf, SPI_SR1_BUSY, 0) < 0) {
			printf("fast_read_burst_cdata:check_status error 1\n");
			result = FAIL;
			goto out;
		}
		write_com(conf, cs_low, sizeof(cs_low));

		fastread_buf[7] = (start_addr >> 16) & 0xFF;
		fastread_buf[11] = (start_addr >> 8) & 0xFF;
		fastread_buf[15] = (start_addr) & 0xFF;
		write_com(conf, fastread_buf, sizeof(fastread_buf));

		cc = read_com(conf, DBG_BUF, sizeof(DBG_BUF));
		if (cc != sizeof(DBG_BUF)) {
			fprintf(stderr, "%s: partial read %zd\n", __func__, cc);
			result = FAIL;
			goto out;
		}

		progress_percent = (++j * 100) / total_size;

		if (conf->read_start_addr != NO_READ) {
			fwrite(DBG_BUF, 1, read_count, pW);
			print_delta("Saving...", &prev_percent,
				    progress_percent);
		} else {
			if (check_erased) {
				count = memcmp(&DBG_BUF, &allff, 256);
				print_delta("Checking...", &prev_percent,
					    progress_percent);
			} else {
				count = memcmp(&C_Data[start_addr], DBG_BUF,
					       256);
				print_delta("Verifying...", &prev_percent,
					    progress_percent);
			}

			if (count) {
				printf("fast_read_burst_cdata ERR\n");
				hexdump(DBG_BUF, 256);
				result = FAIL;
				goto out;
			}
		}
		write_com(conf, cs_high, sizeof(cs_high));
		if (check_status(conf, SPI_SR1_BUSY, 0) < 0) {
			printf("fast_read_burst_cdata:check_status error 2\n");
			result = FAIL;
			goto out;
		}
		start_addr += read_count;
	}
out:
	write_com(conf, disable_follow_mode, sizeof(disable_follow_mode));

	if (conf->read_start_addr != NO_READ)
		fclose(pW);

	return result;
}

static void set_prog_addr(unsigned long addr, uint8_t *pp_buf)
{
	pp_buf[7] = (addr >> 16) & 0xFF;
	pp_buf[11] = (addr >> 8) & 0xFF;
	pp_buf[15] = (addr) & 0xFF;
}

static int program_page(struct itecomdbgr_config *conf,
			unsigned long flash_offset, const uint8_t *wr_data,
			int wr_count)
{
	uint8_t pp_buf[20] = {
		W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       SPI_PP,
		W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
		W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
		W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
		W_CMD_PORT, DBUS_256W_DATA, W_BURST_DATA_PORT, 0xFF
	};
	int i;

	/*
	 * We know the page has been erased to 0xff.
	 * Can we skip this page?
	 */
	for (i = 0; i < wr_count; ++i) {
		if (wr_data[i] != 0xff)
			break;
	}
	if (i == wr_count)
		return SUCCESS;

	/* Need to program page after all. */

	/* Check Write Enable Latch on */
	if (spi_sr1_wel(conf) != SUCCESS) {
		printf("%s: check_status WEL err\n", __func__);
		return FAIL;
	}

	write_com(conf, cs_low, sizeof(cs_low));
	set_prog_addr(flash_offset, pp_buf);
	write_com(conf, pp_buf, sizeof(pp_buf));
	write_com(conf, wr_data, wr_count);
	write_com(conf, cs_high, sizeof(cs_high));

	/* Check WIP bit off */
	if (check_status(conf, SPI_SR1_BUSY, 0) < 0) {
		printf("%s: check_status WIP err\n", __func__);
		return FAIL;
	}

	return SUCCESS;
}

static int page_program_burst_v2(struct itecomdbgr_config *conf,
				 uint8_t *wr_data)
{
	int result = SUCCESS;
	unsigned int j = 0;
	unsigned long start_addr = conf->update_start_addr;
	unsigned long end_addr = conf->update_end_addr;
	int write_count;
	int total_pages = (end_addr - start_addr) / conf->page_size;
	int prev_percent = -1;
	int progress_percent;

	write_com(conf, enable_follow_mode, sizeof(enable_follow_mode));

	while (start_addr < end_addr) {
		if ((end_addr - start_addr) >= conf->page_size)
			write_count = conf->page_size;
		else
			write_count = end_addr - start_addr;

		if (program_page(conf, start_addr, &wr_data[start_addr],
				 write_count) != SUCCESS) {
			result = FAIL;
			goto out;
		}

		start_addr += conf->page_size;

		progress_percent = (++j * 100) / total_pages;
		print_delta("Programming...", &prev_percent, progress_percent);
	}
out:
	write_com(conf, disable_follow_mode, sizeof(disable_follow_mode));
	return result;
}

static int write_flash(struct itecomdbgr_config *conf)
{
	int result = SUCCESS;
	if ((result = page_program_burst_v2(conf, conf->g_writebuf)) != 0) {
		printf("write_flash : error\n");
	}
	printf("\n");
	return result;
}

static int check_flash(struct itecomdbgr_config *conf)
{
	int result = SUCCESS;

	if ((result = fast_read_burst_cdata(conf, NULL, 1)) != SUCCESS) {
		printf("check_flash : error\n");
	}
	printf("\n");
	return result;
}

static int verify_flash(struct itecomdbgr_config *conf)
{
	int result = SUCCESS;

	if ((result = fast_read_burst_cdata(conf, conf->g_writebuf, 0)) !=
	    SUCCESS) {
		printf("verify_flash : error\n");
		result = FAIL;
	}
	printf("\n");
	return result;
}

static int read_flash(struct itecomdbgr_config *conf)
{
	int result = SUCCESS;

	conf->update_start_addr = conf->read_start_addr;
	conf->update_end_addr = conf->read_start_addr + conf->read_range;

	if ((result = fast_read_burst_cdata(conf, NULL, true)) != SUCCESS) {
		printf("read_flash : error\n");
		result = FAIL;
	}
	printf("\n");
	return result;
}

static void enter_uart_dbgr_mode(struct itecomdbgr_config *conf)
{
	uint8_t en_dbgr_buf[7] = { 0x00, W_CMD_PORT,	    0x00, W_DATA_PORT,
				   0x00, W_BURST_DATA_PORT, 0x00 };
	write_com(conf, en_dbgr_buf, sizeof(en_dbgr_buf));
	msleep(5);
}

static int uart_app(struct itecomdbgr_config *conf)
{
	struct termios tty;
	uint8_t dbgr_reset_buf[4] = { W_CMD_PORT, 0x27, W_DATA_PORT, 0x80 };
	int return_status = 0;

	if (conf->device_name == NULL) {
		fprintf(stderr,
			"open device fail , please set the device name");
		return -1;
	}

	conf->g_fd = open(conf->device_name, O_RDWR | O_NOCTTY);

	if (conf->g_fd < 0) {
		perror("open");
		return -1;
	}

	if (tcgetattr(conf->g_fd, &tty) != 0) {
		perror("tcgetattr");
		return -1;
	}

	tty_saved = tty;
	tty_saved_fd = dup(conf->g_fd);

	tty.c_cflag |= PARENB;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;
	tty.c_cflag &= ~CRTSCTS;
	tty.c_cflag |= CREAD | CLOCAL;
	tty.c_lflag &= ~ICANON;
	tty.c_lflag &= ~ECHO;
	tty.c_lflag &= ~ECHOE;
	tty.c_lflag &= ~ECHONL;
	tty.c_lflag &= ~ISIG;
	tty.c_iflag &= ~(IXON | IXOFF | IXANY);
	tty.c_iflag &=
		~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
	tty.c_oflag &= ~OPOST;
	tty.c_oflag &= ~ONLCR;

	tty.c_cc[VTIME] = 10;
	tty.c_cc[VMIN] = 0;

	if (conf->baudrate != 3000000) {
		/* set baud rate to 115200 */
		cfsetospeed(&tty, B115200);
		cfsetispeed(&tty, B115200);
	} else {
		cfsetospeed(&tty, B3000000);
		cfsetispeed(&tty, B3000000);
	}
	/*  apply settings to serial port */
	if (tcsetattr(conf->g_fd, TCSANOW, &tty) != 0) {
		perror("tcsetattr");
	}
	flush_com(conf);

	int tries = 0;
	while (++tries < 25) {
		flush_com(conf);
		if (conf->g_steps == STEPS_TEST) {
			enter_uart_dbgr_mode(conf);
			read_id_2(conf);
			conf->g_steps = STEPS_EXIT;
		}

		if (conf->g_steps == STEPS_NORMAL) {
			enter_uart_dbgr_mode_and_set_nack_mode(conf);

			/* dbgr reset */
			write_com(conf, dbgr_reset_buf, sizeof(dbgr_reset_buf));

			if (getchipid(conf) == SUCCESS) {
				/* Reset UART1*/
				wr_reg(conf, 0xF02011, 1);

				wr_reg(conf, 0xF01618, 0xFF);
				wr_reg(conf, 0xF01619, 0xFF);
				read_id_2(conf);
			}
			flush_com(conf);
		}

		if (conf->g_steps == STEPS_EXIT) {
			flush_com(conf);
			break;
		}

		msleep(70);
	}

	dbgr_disable_protect_path(conf);

	switch (conf->eflash_type) {
	case EFLASH_TYPE_8315:
		conf->sector_erase_pages = 4;
		conf->spi_cmd_sector_erase = SPI_CMD_SECTOR_ERASE_1K;
		conf->sector_size = 1024;
		break;
	case EFLASH_TYPE_KGD:
		conf->sector_erase_pages = 16;
		conf->spi_cmd_sector_erase = SPI_CMD_SECTOR_ERASE_4K;
		conf->sector_size = 4096;
		break;
	default:
		printf("Invalid EFLASH TYPE!\n");
		return_status = -1;
		goto out;
	}

	if (conf->read_start_addr != NO_READ) {
		read_flash(conf);
		goto out;
	}

	if (erase_flash(conf)) {
		return_status = -1;
		goto out;
	}

	if (!conf->noverify) {
		if (check_flash(conf)) {
			return_status = -1;
			goto out;
		}
	}

	if (write_flash(conf)) {
		return_status = -1;
		goto out;
	}

	if (!conf->noverify) {
		if (verify_flash(conf)) {
			return_status = -1;
			goto out;
		}
	}

out:

	/* dbgr reset */
	write_com(conf, dbgr_reset_buf, sizeof(dbgr_reset_buf));
	flush_com(conf);
	tcsetattr(tty_saved_fd, TCSANOW, &tty_saved);
	close(tty_saved_fd);
	tty_saved_fd = -1;
	close(conf->g_fd);
	return return_status;
}

static void exit_handler(int signum)
{
	if (tty_saved_fd >= 0) {
		tcsetattr(tty_saved_fd, TCSANOW, &tty_saved);
		close(tty_saved_fd);
	}
	_exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int r = 0;
	int option_index = 0;
	int c;
	const char *optstring = "f:d:b:hnr:R:";
	struct option long_options[] = {
		{ "filename", required_argument, NULL, 'f' },
		{ "device", required_argument, NULL, 'd' },
		{ "baudrate", required_argument, NULL, 'b' },
		{ "no-verify", no_argument, NULL, 'n' },
		{ "readfile", required_argument, NULL, 'r' },
		{ "Range", required_argument, NULL, 'R' },
		{ 0, 0, 0, 0 }
	};

	/* Set initial value , it will update the real value later */
	struct itecomdbgr_config conf = { .g_steps = STEPS_NORMAL,
					  .g_flash_size = 0x100000,
					  .g_blk_size = 65536,
					  .g_blk_no = 16,
					  .page_size = 256,
					  .sector_size = 4096,
					  .baudrate = 115200,
					  .read_start_addr = NO_READ,
					  .read_range = 0, /* update later*/
					  .noverify = 0,
					  .device_name = NULL,
					  .file_name = NULL,
					  .eflash_size_in_k = 0,
					  .eflash_type = 0xFF };

	while (1) {
		c = getopt_long(argc, argv, optstring, long_options,
				&option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'f':
			conf.file_name = optarg;
			break;
		case 'r':
			conf.read_file_name = optarg;
			break;
		case 'R':
			conf.read_start_addr =
				strtol(argv[optind - 1], (char **)0, 0);
			conf.read_range = strtol(argv[optind], (char **)0, 0);
			break;

		case 'd':
			conf.device_name = optarg;
			break;
		case 'b':
			conf.baudrate = atol(optarg);
			break;
		case 'n':
			conf.noverify = 1;
			break;
		case 'h':
		default:
			printf("\n");
			printf("Usage:\n");
			printf("	-f [fw filename]\n");
			printf("	-d [device name]\n");
			printf("	-b [baudrate]\n");
			printf("	-n : no verify\n");
			printf("	-r : [read filename]\n");
			printf("	-R : [read start addr] [length]\n");
			printf("Example :\n");
			printf("    %s -f ec.bin -d /dev/ttyUSB3\n", argv[0]);
			printf("    %s -f ec.bin -d /dev/ttyUSB3 -n\n",
			       argv[0]);
			printf("    %s -R 0 0x100000\n", argv[0]);
			exit(1);
		}
	}

	if ((conf.baudrate != 115200) && (conf.baudrate != 3000000)) {
		printf("UART Baudrate only support 115200  or 3M\n");
		return 0;
	}

	if ((conf.read_file_name != NULL) &&
	    (conf.read_start_addr == NO_READ)) {
		/* User requested to read the entire image. */
		conf.read_start_addr = 0;
	}

	if ((conf.file_name == NULL) && (conf.read_start_addr == NO_READ) &&
	    (conf.read_range == 0)) {
		printf("choose a file to flash..\n");
		return 0;
	}

	r = init_file(&conf);
	if (r) {
		printf("Open file error\n");
		exit(1);
	}

	signal(SIGHUP, exit_handler);
	signal(SIGINT, exit_handler);
	signal(SIGQUIT, exit_handler);
	signal(SIGTERM, exit_handler);

	r = uart_app(&conf);
	exit_file(&conf);
	return (r != 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
