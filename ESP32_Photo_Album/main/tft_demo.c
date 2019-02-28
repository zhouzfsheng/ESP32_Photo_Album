/* TFT demo

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <time.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "tftspi.h"
#include "tft.h"
#include "spiffs_vfs.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "freertos/event_groups.h"
#include "esp_attr.h"
#include <sys/time.h>
#include <unistd.h>
#include "lwip/err.h"
#include "apps/sntp/sntp.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "pig.h"
// ==========================================================
// Define which spi bus to use TFT_VSPI_HOST or TFT_HSPI_HOST
#define SPI_BUS TFT_HSPI_HOST
// ==========================================================


static int _demo_pass = 0;
static uint8_t doprint = 1;
static uint8_t run_gs_demo = 0; // Run gray scale demo if set to 1
static struct tm* tm_info;
static char tmp_buff[64];
static time_t time_now, time_last = 0;
static const char *file_fonts[3] = {"/spiffs/fonts/DotMatrix_M.fon", "/spiffs/fonts/Ubuntu.fon", "/spiffs/fonts/Grotesk24x48.fon"};

#define GDEMO_TIME 1000
#define GDEMO_INFO_TIME 5000

//==================================================================================

static const char tag[] = "[TFT Demo]";


static EventGroupHandle_t wifi_event_group;


const int CONNECTED_BIT = 0x00000001;


/******************************************************
 * Function: display string
 * Parameters:
 * Return: void
 * Other: void
 *****************************************************/
void lcd_display_picture(const unsigned char *picture)
{
    uint16_t x = 0,y = 0;
    uint16_t i = 0;
    color_t pixel = {0};

    for (i = 0; i < 20480; i++)
    {
        pixel.r = *(picture);
        picture++;
        pixel.g = *(picture);
        picture++;
        pixel.b = *(picture);
        picture++;

        TFT_drawPixel(x, y, pixel, 1);
        if ((++x) >= 160)
        {
            x = 0;
            y ++;
        }
    }
}

//------------------------------------------------------------
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
	switch(event->event_id) {
	case SYSTEM_EVENT_STA_START:
		esp_wifi_connect();
		break;
	case SYSTEM_EVENT_STA_GOT_IP:
		xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		esp_wifi_connect();
		xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
		break;
	default:
		break;
	}
	return ESP_OK;
}

//-------------------------------
static void initialise_wifi(void)
{
	tcpip_adapter_init();
	wifi_event_group = xEventGroupCreate();
	ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
	ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
	wifi_config_t wifi_config = {
		.sta = {
			.ssid = "kyzh-hw-TEST",
			.password = "KYZH12345678",
		},
	};
	ESP_LOGI(tag, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
	ESP_ERROR_CHECK( esp_wifi_start() );
}

//-------------------------------
static void initialize_sntp(void)
{
	ESP_LOGI(tag, "Initializing SNTP");
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, "ntp1.aliyun.com");
	sntp_init();
}

//--------------------------
static int obtain_time(void)
{
	int res = 1;
	initialise_wifi();
	xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

	initialize_sntp();

	// wait for time to be set
	int retry = 0;
	const int retry_count = 20;

	time(&time_now);
	tm_info = localtime(&time_now);

	while(tm_info->tm_year < (2016 - 1900) && ++retry < retry_count) {
		//ESP_LOGI(tag, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
		sprintf(tmp_buff, "Wait %0d/%d", retry, retry_count);
		TFT_print(tmp_buff, CENTER, LASTY);
		vTaskDelay(500 / portTICK_RATE_MS);
		time(&time_now);
		tm_info = localtime(&time_now);
	}
	if (tm_info->tm_year < (2016 - 1900)) {
		ESP_LOGI(tag, "System time NOT set.");
		res = 0;
	}
	else {
		ESP_LOGI(tag, "System time is set.");
	}

	//ESP_ERROR_CHECK( esp_wifi_stop() );
	return res;
}

//----------------------
static void _checkTime()
{
	time(&time_now);
	if (time_now > time_last) {
		color_t last_fg, last_bg;
		time_last = time_now;
		tm_info = localtime(&time_now);
		sprintf(tmp_buff, "%02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);

		TFT_saveClipWin();
		TFT_resetclipwin();

		Font curr_font = cfont;
		last_bg = _bg;
		last_fg = _fg;
		_fg = TFT_YELLOW;
		_bg = (color_t){ 64, 64, 64 };
		TFT_setFont(DEFAULT_FONT, NULL);

		TFT_fillRect(1, _height-TFT_getfontheight()-8, _width-3, TFT_getfontheight()+6, _bg);
		TFT_print(tmp_buff, CENTER, _height-TFT_getfontheight()-5);

		cfont = curr_font;
		_fg = last_fg;
		_bg = last_bg;

		TFT_restoreClipWin();
	}
}


//---------------------
static int Wait(int ms)
{
	uint8_t tm = 1;
	if (ms < 0) {
		tm = 0;
		ms *= -1;
	}
	if (ms <= 50) {
		vTaskDelay(ms / portTICK_RATE_MS);
		//if (_checkTouch()) return 0;
	}
	else {
		for (int n=0; n<ms; n += 50) {
			vTaskDelay(50 / portTICK_RATE_MS);
			if (tm) _checkTime();
			//if (_checkTouch()) return 0;
		}
	}
	return 1;
}

static void _dispTime()
{
	Font curr_font = cfont;
	if(_width < 240) TFT_setFont(DEF_SMALL_FONT, NULL);
	else TFT_setFont(DEFAULT_FONT, NULL);

	time(&time_now);
	time_last = time_now;
	tm_info = localtime(&time_now);
	sprintf(tmp_buff, "%02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
	TFT_print(tmp_buff, CENTER, _height-TFT_getfontheight()-5);

	cfont = curr_font;
}


static void disp_header(char *info)
{
	TFT_fillScreen(TFT_BLACK);
	TFT_resetclipwin();

	_fg = TFT_YELLOW;
	_bg = (color_t){ 64, 64, 64 };

	if (_width < 240) TFT_setFont(DEF_SMALL_FONT, NULL);
	else TFT_setFont(DEFAULT_FONT, NULL);
	TFT_fillRect(0, 0, _width-1, TFT_getfontheight()+8, _bg);
	TFT_drawRect(0, 0, _width-1, TFT_getfontheight()+8, TFT_CYAN);

	TFT_fillRect(0, _height-TFT_getfontheight()-9, _width-1, TFT_getfontheight()+8, _bg);
	TFT_drawRect(0, _height-TFT_getfontheight()-9, _width-1, TFT_getfontheight()+8, TFT_CYAN);

	TFT_print(info, CENTER, 4);
	_dispTime();

	_bg = TFT_BLACK;
	TFT_setclipwin(0,TFT_getfontheight()+9, _width-1, _height-TFT_getfontheight()-10);
}

static void update_header(char *hdr, char *ftr)
{
	color_t last_fg, last_bg;

	TFT_saveClipWin();
	TFT_resetclipwin();

	Font curr_font = cfont;
	last_bg = _bg;
	last_fg = _fg;
	_fg = TFT_YELLOW;
	_bg = (color_t){ 64, 64, 64 };
	if (_width < 240) TFT_setFont(DEF_SMALL_FONT, NULL);
	else TFT_setFont(DEFAULT_FONT, NULL);

	if (hdr) {
		TFT_fillRect(1, 1, _width-3, TFT_getfontheight()+6, _bg);
		TFT_print(hdr, CENTER, 4);
	}

	if (ftr) {
		TFT_fillRect(1, _height-TFT_getfontheight()-8, _width-3, TFT_getfontheight()+6, _bg);
		if (strlen(ftr) == 0) _dispTime();
		else TFT_print(ftr, CENTER, _height-TFT_getfontheight()-5);
	}

	cfont = curr_font;
	_fg = last_fg;
	_bg = last_bg;

	TFT_restoreClipWin();
}


void LCD_init()
{
	// ========  PREPARE DISPLAY INITIALIZATION  =========

	esp_err_t ret;

	tft_disp_type = DEFAULT_DISP_TYPE;

	_width = DEFAULT_TFT_DISPLAY_WIDTH;  // smaller dimension
	_height = DEFAULT_TFT_DISPLAY_HEIGHT; // larger dimension
	max_rdclock = 8000000;

	TFT_PinsInit();

	spi_lobo_device_handle_t spi;
	
	spi_lobo_bus_config_t buscfg={
		.miso_io_num=PIN_NUM_MISO,				// set SPI MISO pin
		.mosi_io_num=PIN_NUM_MOSI,				// set SPI MOSI pin
		.sclk_io_num=PIN_NUM_CLK,				// set SPI CLK pin
		.quadwp_io_num=-1,
		.quadhd_io_num=-1,
		.max_transfer_sz = 6*1024,
		};
		spi_lobo_device_interface_config_t devcfg={
		.clock_speed_hz=8000000,			   // Initial clock out at 8 MHz
		.mode=0,								// SPI mode 0
			.spics_io_num=-1,						// we will use external CS pin
		.spics_ext_io_num=PIN_NUM_CS,			// external CS pin
		.flags=LB_SPI_DEVICE_HALFDUPLEX,		// ALWAYS SET  to HALF DUPLEX MODE!! for display spi
	};

	// ====================================================================================================================

	vTaskDelay(500 / portTICK_RATE_MS);
	printf("\r\n==============================\r\n");
	printf("TFT display DEMO, LoBo 11/2017\r\n");
	printf("==============================\r\n");
	printf("Pins used: miso=%d, mosi=%d, sck=%d, cs=%d\r\n", PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);

	printf("==============================\r\n\r\n");

	// ==================================================================
	// ==== Initialize the SPI bus and attach the LCD to the SPI bus ====

	ret=spi_lobo_bus_add_device(SPI_BUS, &buscfg, &devcfg, &spi);
	assert(ret==ESP_OK);
	printf("SPI: display device added to spi bus (%d)\r\n", SPI_BUS);
	disp_spi = spi;

	// ==== Test select/deselect ====
	ret = spi_lobo_device_select(spi, 1);
	assert(ret==ESP_OK);
	ret = spi_lobo_device_deselect(spi);
	assert(ret==ESP_OK);

	printf("SPI: attached display device, speed=%u\r\n", spi_lobo_get_speed(spi));
	printf("SPI: bus uses native pins: %s\r\n", spi_lobo_uses_native_pins(spi) ? "true" : "false");

	// ================================
	// ==== Initialize the Display ====

	printf("SPI: display init...\r\n");
	TFT_display_init();
	printf("OK\r\n");

	
	// ---- Detect maximum read speed ----
	max_rdclock = find_rd_speed();
	printf("SPI: Max rd speed = %u\r\n", max_rdclock);

	// ==== Set SPI clock used for display operations ====
	spi_lobo_set_speed(spi, DEFAULT_SPI_CLOCK);
	printf("SPI: Changed speed to %u\r\n", spi_lobo_get_speed(spi));

	printf("\r\n---------------------\r\n");
	printf("Graphics demo started\r\n");
	printf("---------------------\r\n");

	font_rotate = 0;
	text_wrap = 0;
	font_transparent = 0;
	font_forceFixed = 0;
	gray_scale = 0;
	TFT_setGammaCurve(DEFAULT_GAMMA_CURVE);
	TFT_setRotation(LANDSCAPE_FLIP);
	TFT_setFont(DEFAULT_FONT, NULL);
	TFT_resetclipwin();
}

void wifi_connect(void)
{
	ESP_ERROR_CHECK( nvs_flash_init() );

	// ===== Set time zone ======
	setenv("TZ", "CET-1CEST", 0);
	tzset();
	// ==========================

	disp_header("GET NTP TIME");

	time(&time_now);
	tm_info = localtime(&time_now);

	if (tm_info->tm_year < (2016 - 1900)) {
		ESP_LOGI(tag, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
		_fg = TFT_CYAN;
		TFT_print("Time is not set yet", CENTER, CENTER);
		TFT_print("Connecting to WiFi", CENTER, LASTY+TFT_getfontheight()+2);
		TFT_print("Getting time over NTP", CENTER, LASTY+TFT_getfontheight()+2);
		_fg = TFT_YELLOW;
		TFT_print("Wait", CENTER, LASTY+TFT_getfontheight()+2);
		if (obtain_time()) {
			_fg = TFT_GREEN;
			TFT_print("System time is set.", CENTER, LASTY);
		}
		else {
			_fg = TFT_RED;
			TFT_print("ERROR.", CENTER, LASTY);
		}
		time(&time_now);
		update_header(NULL, "");
		Wait(-2000);
	}
}

#define LINE 12
uint16_t line = 0;
void tftprintf(const char *date)
{
	uint16_t width = DEFAULT_TFT_DISPLAY_WIDTH - LINE;
	char tft_buf[64];
	line = line + LINE;
	if(line>width)
	{
		line = 0;TFT_fillWindow(TFT_BLACK);
	}
	sprintf(tft_buf,"%s",date);
	TFT_print(tft_buf, 0, line);
	memset(tft_buf,0,sizeof(tft_buf));
}


#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"


#define WEB_SERVER "192.168.0.5"
#define WEB_PORT 80
#define WEB_URL "http://example.com/"

static const char *TAG = "example";

static const char *REQUEST = "GET " WEB_URL " HTTP/1.0\r\n"
	"Host: "WEB_SERVER"\r\n"
	"User-Agent: esp-idf/1.0 esp32\r\n"
	"\r\n";

// Handle of the wear levelling library instance
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

// Mount path for the partition
const char *base_path = "/spiflash";


void file_display()
{
    uint16_t x = 0,y = 0;
    uint16_t i = 0,j = 0,n = 0,m=0,r=0;
    color_t pixel = {0};
    char pmd_buf[1024] = {0};

    // xQueueReceive(LCD_Show_Queue,(void *)&Rx_Display,1000*LCD_F5_FREQ/portTICK_RATE_MS )

	FILE *f = fopen("/spiflash/pig.bin", "rb");
	if (f == NULL) {
		ESP_LOGI(TAG,"Failed to open file for writing");
		return;
	}

	while (m<61440)
	{
		fseek(f,m,0);
		r = fread(pmd_buf, sizeof(char),1023, f);

	    for (i = 0; i < r; )
	    {
	        pixel.r = pmd_buf[i];
	        i++;
	        pixel.g = pmd_buf[i];
	        i++;
	        pixel.b = pmd_buf[i];
	        i++;
	        TFT_drawPixel(x, y, pixel, 1);
	        if ((++x) >= 160)
	        {
	            x = 0;
	            y ++;
	        }
	    }
	    m = m + r;
	}

	fclose(f);

	// while(1) {
	// 	vTaskDelay(1000);
	// }
}

//void http_get_task(void *pvParameters)
void http_get_task(void)
{
	const struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	struct addrinfo *res;
	struct in_addr *addr;
	int s, r;
	char recv_buf[2048];
	uint16_t packet_length = 0;

	char tft_buf[64];int i;

	ESP_LOGI(TAG, "Mounting FAT filesystem");
	// To mount device we need name of device partition, define base_path
	// and allow format partition in case if it is new one and was not formated before
	const esp_vfs_fat_mount_config_t mount_config = {
	.max_files = 4,
	.format_if_mount_failed = true
	};
	esp_err_t err = esp_vfs_fat_spiflash_mount(base_path, "storage", &mount_config, &s_wl_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to mount FATFS (0x%x)", err);
		return;
	}

	while(1) 
	{

		int err = getaddrinfo(WEB_SERVER, "808", &hints, &res);

		if(err != 0 || res == NULL) {
			ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			continue;
		}

		addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
		ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

		s = socket(res->ai_family, res->ai_socktype, 0);
		if(s < 0) {
			ESP_LOGE(TAG, "... Failed to allocate socket.");
			freeaddrinfo(res);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			continue;
		}
		ESP_LOGI(TAG, "... allocated socket");

		if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
			ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
			close(s);
			freeaddrinfo(res);
			vTaskDelay(4000 / portTICK_PERIOD_MS);
			continue;
		}

		ESP_LOGI(TAG, "... connected");

		freeaddrinfo(res);

		while(1)
		{
			if (write(s, REQUEST, strlen(REQUEST)) < 0) {
				ESP_LOGE(TAG, "... socket send failed");
				close(s);
				vTaskDelay(4000 / portTICK_PERIOD_MS);
				continue;
			}
			ESP_LOGI(TAG, "... socket send success");

			struct timeval receiving_timeout;
			receiving_timeout.tv_sec = 5;
			receiving_timeout.tv_usec = 0;
			if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
					sizeof(receiving_timeout)) < 0) {
				ESP_LOGE(TAG, "... failed to set socket receiving timeout");
				close(s);
				vTaskDelay(4000 / portTICK_PERIOD_MS);
				continue;
			}
			ESP_LOGI(TAG, "... set socket receiving timeout success");

			ESP_LOGI(TAG, "Opening file");

			remove("/spiflash/pig.bin");
			FILE *f = fopen("/spiflash/pig.bin", "wb");
			if (f == NULL) {
				ESP_LOGE(TAG, "Failed to open file for writing");
				return;
			}

			// Read HTTP response
			do {
				bzero(recv_buf, sizeof(recv_buf));
				r = read(s, recv_buf, sizeof(recv_buf)-1);
				if(r!=-1)
				{
					// sprintf(tft_buf,"length:%d...",packet_length);
					fseek(f,packet_length, 0);
					fwrite(recv_buf,sizeof(char),r,f);
					packet_length = packet_length + r;
					ESP_LOGE(TAG, "packet_length %d",packet_length);
					if(packet_length == 61440)
					{
						break;
					}
				}
			} while(r > 0);
		
			fclose(f);

			while(packet_length >= 61440)
			{
				file_display();
				//lcd_display_picture(gImage_pig);
				packet_length = 0;
			}

			vTaskDelay(5000);
		}

		// if (write(s, REQUEST, strlen(REQUEST)) < 0) {
		// 	ESP_LOGE(TAG, "... socket send failed");
		// 	close(s);
		// 	vTaskDelay(4000 / portTICK_PERIOD_MS);
		// 	continue;
		// }
		// ESP_LOGI(TAG, "... socket send success");

		// struct timeval receiving_timeout;
		// receiving_timeout.tv_sec = 5;
		// receiving_timeout.tv_usec = 0;
		// if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
		// 		sizeof(receiving_timeout)) < 0) {
		// 	ESP_LOGE(TAG, "... failed to set socket receiving timeout");
		// 	close(s);
		// 	vTaskDelay(4000 / portTICK_PERIOD_MS);
		// 	continue;
		// }
		// ESP_LOGI(TAG, "... set socket receiving timeout success");

		// ESP_LOGI(TAG, "Opening file");

		// remove("/spiflash/pig.bin");
		// FILE *f = fopen("/spiflash/pig.bin", "wb");
		// if (f == NULL) {
		// 	ESP_LOGE(TAG, "Failed to open file for writing");
		// 	return;
		// }

		// // Read HTTP response
		// do {
		// 	bzero(recv_buf, sizeof(recv_buf));
		// 	r = read(s, recv_buf, sizeof(recv_buf)-1);
		// 	if(r!=-1)
		// 	{
		// 		sprintf(tft_buf,"length:%d...",packet_length);
		// 		fseek(f,packet_length, 0);
		// 		fwrite(recv_buf,sizeof(char),r,f);
		// 		packet_length = packet_length + r;
		// 		if(packet_length == 61440)
		// 		{
		// 			break;
		// 		}
		// 	}
		// } while(r > 0);
	
		// fclose(f);

		ESP_LOGI(TAG, "... done reading from socket. Last read return=%d errno=%d\r\n", r, errno);
		close(s);

		// while(packet_length >= 61440)
		// {
		// 	file_display();
		// 	//lcd_display_picture(gImage_pig);
		// 	packet_length = 0;
		// }

		for(int countdown = 10; countdown >= 0; countdown--) {
			ESP_LOGI(TAG, "%d... ", countdown);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
		}

		ESP_LOGI(TAG, "Starting again!");
	}
}


//=============
void app_main()
{
	LCD_init();
	wifi_connect();
	TFT_setRotation(LANDSCAPE_FLIP);
	lcd_display_picture(gImage_pig);
	// TFT_fillWindow(TFT_BLACK);
	// http_get_task();

	xTaskCreate(http_get_task, "http_get_task", 8192, NULL, 11, NULL);
}


