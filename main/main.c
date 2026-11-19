#include "rgb_lcd_port.h" // Header for Waveshare RGB LCD driver
#include "gui_paint.h"    // Header for graphical drawing functions
#include "jpeg_decoder.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_jpeg_dec.h"
#include <string.h>

extern const uint8_t test_jpg_start[] asm("_binary_test_jpg_start");
extern const uint8_t test_jpg_end[] asm("_binary_test_jpg_end");
extern const uint8_t gray_480_jpg_start[] asm("_binary_gray_480_jpg_start");
extern const uint8_t gray_480_jpg_end[] asm("_binary_gray_480_jpg_end");
extern const uint8_t gray_800_jpg_start[] asm("_binary_gray_800_jpg_start");
extern const uint8_t gray_800_jpg_end[] asm("_binary_gray_800_jpg_end");
extern const uint8_t gray_1024_jpg_start[] asm("_binary_gray_1024_jpg_start");
extern const uint8_t gray_1024_jpg_end[] asm("_binary_gray_1024_jpg_end");
extern const uint8_t crop_512_jpg_start[] asm("_binary_crop_512_jpg_start");
extern const uint8_t crop_512_jpg_end[] asm("_binary_crop_512_jpg_end");
extern const uint8_t crop_1024_jpg_start[] asm("_binary_crop_1024_jpg_start");
extern const uint8_t crop_1024_jpg_end[] asm("_binary_crop_1024_jpg_end");

#define ROTATE ROTATE_0 // rotate = 0, 90, 180, 270

static void bench(const char *name, const uint8_t *start, const uint8_t *end,
                  uint8_t *buf, size_t buf_size, void *work, size_t work_size,
                  esp_lcd_panel_handle_t panel)
{
    esp_jpeg_image_cfg_t cfg = {
        .indata = (uint8_t *)start,
        .indata_size = end - start,
        .outbuf = buf,
        .outbuf_size = buf_size,
        .out_format = JPEG_IMAGE_FORMAT_RGB565,
        .out_scale = JPEG_IMAGE_SCALE_0,
        .advanced.working_buffer = work,
        .advanced.working_buffer_size = work_size,
    };
    esp_jpeg_image_output_t out;

    int64_t t0 = esp_timer_get_time();
    esp_err_t err = esp_jpeg_decode(&cfg, &out);
    int64_t t1 = esp_timer_get_time();
    esp_lcd_panel_draw_bitmap(panel, 0, 0, out.width, out.height, buf);
    int64_t t2 = esp_timer_get_time();

    printf("%-10s %4dx%-4d %6d B  decode %4lld ms  draw %4lld ms  %s\n",
           name, out.width, out.height, (int)(end - start),
           (t1 - t0) / 1000, (t2 - t1) / 1000, esp_err_to_name(err));
}

static void bench_new(const char *name, const uint8_t *start, const uint8_t *end,
                      uint8_t *buf, esp_lcd_panel_handle_t panel)
{
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;

    jpeg_dec_handle_t dec = NULL;
    jpeg_dec_io_t io = {
        .inbuf = (uint8_t *)start,
        .inbuf_len = end - start,
        .outbuf = buf,
    };
    jpeg_dec_header_info_t info = {0};

    int64_t t0 = esp_timer_get_time();
    jpeg_error_t err = jpeg_dec_open(&config, &dec);
    if (err == JPEG_ERR_OK)
        err = jpeg_dec_parse_header(dec, &io, &info);
    if (err == JPEG_ERR_OK)
        err = jpeg_dec_process(dec, &io);
    if (dec)
        jpeg_dec_close(dec);
    int64_t t1 = esp_timer_get_time();

    if (err == JPEG_ERR_OK)
        esp_lcd_panel_draw_bitmap(panel, 0, 0, info.width, info.height, buf);
    int64_t t2 = esp_timer_get_time();

    printf("%-10s %4dx%-4d %6d B  decode %4lld ms  draw %4lld ms  err %d\n",
           name, info.width, info.height, (int)(end - start),
           (t1 - t0) / 1000, (t2 - t1) / 1000, err);
}

static void bench_block(const char *name, const uint8_t *start, const uint8_t *end,
                        uint16_t *fb, esp_lcd_panel_handle_t panel)
{
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;
    config.block_enable = true;

    jpeg_dec_handle_t dec = NULL;
    jpeg_dec_io_t io = {.inbuf = (uint8_t *)start, .inbuf_len = end - start};
    jpeg_dec_header_info_t info = {0};
    uint8_t *block = NULL;
    uint32_t *line = NULL;
    int block_len = 0, count = 0;

    int64_t t0 = esp_timer_get_time();
    jpeg_error_t err = jpeg_dec_open(&config, &dec);
    if (err == JPEG_ERR_OK)
        err = jpeg_dec_parse_header(dec, &io, &info);
    if (err == JPEG_ERR_OK && (info.width * 2 > EXAMPLE_LCD_H_RES || info.height * 2 > EXAMPLE_LCD_V_RES))
        err = JPEG_ERR_INVALID_PARAM; // too big to double
    if (err == JPEG_ERR_OK)
        err = jpeg_dec_get_outbuf_len(dec, &block_len);
    if (err == JPEG_ERR_OK)
        err = jpeg_dec_get_process_count(dec, &count);
    if (err == JPEG_ERR_OK)
    {
        block = heap_caps_aligned_alloc(16, block_len, MALLOC_CAP_INTERNAL);
        line = heap_caps_aligned_alloc(16, EXAMPLE_LCD_H_RES * 2, MALLOC_CAP_INTERNAL);
        if (block == NULL || line == NULL)
            err = JPEG_ERR_NO_MEM;
    }
    io.outbuf = block;

    int y = 0; // which source row we're on
    for (int i = 0; i < count && err == JPEG_ERR_OK; i++)
    {
        err = jpeg_dec_process(dec, &io);          // decode one strip
        int rows = io.out_size / (info.width * 2); // rows in this strip
        uint16_t *src = (uint16_t *)block;

        for (int r = 0; r < rows; r++, y++)
        {
            uint16_t *s = src + r * info.width; // this source row
            for (int x = 0; x < info.width; x++)
            {
                uint32_t p = s[x];
                line[x] = p | (p << 16); // same pixel in both halves
            }
            uint16_t *row = fb + (2 * y) * EXAMPLE_LCD_H_RES;
            memcpy(row, line, info.width * 4);                     // screen row 2*y
            memcpy(row + EXAMPLE_LCD_H_RES, line, info.width * 4); // screen row 2*y+1
        }
    }
    if (dec)
        jpeg_dec_close(dec);
    if (block)
        heap_caps_free(block);
    if (line)
        heap_caps_free(line);
    int64_t t1 = esp_timer_get_time();

    if (err == JPEG_ERR_OK)
        esp_lcd_panel_draw_bitmap(panel, 0, 0, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, fb);
    int64_t t2 = esp_timer_get_time();

    printf("%-10s %4dx%-4d %6d B  decode+2x %4lld ms  draw %4lld ms  err %d\n",
           name, info.width, info.height, (int)(end - start),
           (t1 - t0) / 1000, (t2 - t1) / 1000, err);
}

void app_main()
{
    // Initialize I2C communication and CH422G hardware interface
    DEV_I2C_Init();
    IO_EXTENSION_Init();

    // Initialize the Waveshare ESP32-S3 RGB LCD
    esp_lcd_panel_handle_t panel = waveshare_esp32_s3_rgb_lcd_init();

    // Turn on the LCD backlight
    wavesahre_rgb_lcd_bl_on();
    // Uncomment the following line to turn off the backlight if needed
    // wavesahre_rgb_lcd_bl_off();

    // Allocate memory for the screen's frame buffer
    UDOUBLE Imagesize = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * 2; // Each pixel takes 2 bytes in RGB565
    UBYTE *BlackImage;
    if ((BlackImage = (UBYTE *)jpeg_calloc_align(Imagesize, 16)) == NULL) // Allocate memory
    {
        printf("Failed to apply for black memory...\r\n");
        exit(0); // Exit the program if memory allocation fails
    }

    size_t work_size = 65472;
    void *work_buf = heap_caps_malloc(work_size, MALLOC_CAP_INTERNAL);

    void *fb[2];
    waveshare_get_frame_buffer(&fb[0], &fb[1]);
    int back = 1; // the screen starts on fb[0], so fb[1] is hidden

    if (work_buf == NULL)
        printf("work_buf alloc failed\n");

    bench("color1024", test_jpg_start, test_jpg_end, BlackImage, Imagesize, work_buf, work_size, panel);
    vTaskDelay(pdMS_TO_TICKS(1000));
    bench("gray480", gray_480_jpg_start, gray_480_jpg_end, BlackImage, Imagesize, work_buf, work_size, panel);
    vTaskDelay(pdMS_TO_TICKS(1000));
    bench("gray800", gray_800_jpg_start, gray_800_jpg_end, BlackImage, Imagesize, work_buf, work_size, panel);
    vTaskDelay(pdMS_TO_TICKS(1000));
    bench("gray1024", gray_1024_jpg_start, gray_1024_jpg_end, BlackImage, Imagesize, work_buf, work_size, panel);

    vTaskDelay(pdMS_TO_TICKS(1000));
    bench_new("new1024c", test_jpg_start, test_jpg_end, BlackImage, panel);
    vTaskDelay(pdMS_TO_TICKS(1000));
    bench_new("new480g", gray_480_jpg_start, gray_480_jpg_end, BlackImage, panel);
    vTaskDelay(pdMS_TO_TICKS(1000));
    bench_new("new800g", gray_800_jpg_start, gray_800_jpg_end, BlackImage, panel);
    vTaskDelay(pdMS_TO_TICKS(1000));
    bench_new("new1024g", gray_1024_jpg_start, gray_1024_jpg_end, BlackImage, panel);

    vTaskDelay(pdMS_TO_TICKS(1000));
    bench_new("fb1024c", test_jpg_start, test_jpg_end, fb[back], panel);
    back ^= 1;
    vTaskDelay(pdMS_TO_TICKS(1000));
    bench_new("fb1024g", gray_1024_jpg_start, gray_1024_jpg_end, fb[back], panel);
    back ^= 1;

    vTaskDelay(pdMS_TO_TICKS(1000));
    bench_new("new512g", crop_512_jpg_start, crop_512_jpg_end, BlackImage, panel);
    vTaskDelay(pdMS_TO_TICKS(1000));
    bench_block("blk512x2", crop_512_jpg_start, crop_512_jpg_end, (uint16_t *)fb[back], panel);
    back ^= 1;

    vTaskDelay(pdMS_TO_TICKS(3000)); // time to look at the 512 version
    bench_new("crop1024", crop_1024_jpg_start, crop_1024_jpg_end, fb[back], panel);
    back ^= 1;
}
