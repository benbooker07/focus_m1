#include "rgb_lcd_port.h" // Header for Waveshare RGB LCD driver
#include "gui_paint.h"    // Header for graphical drawing functions
#include "jpeg_decoder.h"
#include "esp_timer.h"

extern const uint8_t test_jpg_start[] asm("_binary_test_jpg_start");
extern const uint8_t test_jpg_end[] asm("_binary_test_jpg_end");

#define ROTATE ROTATE_0 // rotate = 0, 90, 180, 270

void app_main()
{
    // Initialize I2C communication and CH422G hardware interface
    DEV_I2C_Init();
    IO_EXTENSION_Init();

    // Initialize the Waveshare ESP32-S3 RGB LCD
    waveshare_esp32_s3_rgb_lcd_init();

    // Turn on the LCD backlight
    wavesahre_rgb_lcd_bl_on();
    // Uncomment the following line to turn off the backlight if needed
    // wavesahre_rgb_lcd_bl_off();

    // Allocate memory for the screen's frame buffer
    UDOUBLE Imagesize = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * 2; // Each pixel takes 2 bytes in RGB565
    UBYTE *BlackImage;
    if ((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) // Allocate memory
    {
        printf("Failed to apply for black memory...\r\n");
        exit(0); // Exit the program if memory allocation fails
    }

    esp_jpeg_image_cfg_t cfg =
        {
            .indata = (uint8_t *)test_jpg_start,
            .indata_size = test_jpg_end - test_jpg_start,
            .outbuf = BlackImage,
            .outbuf_size = Imagesize,
            .out_format = JPEG_IMAGE_FORMAT_RGB565,
            .out_scale = JPEG_IMAGE_SCALE_0,
    };

    esp_jpeg_image_output_t out;

    int64_t t0 = esp_timer_get_time();
    esp_err_t err = esp_jpeg_decode(&cfg, &out);
    int64_t t1 = esp_timer_get_time();
    wavesahre_rgb_lcd_display(BlackImage);
    int64_t t2 = esp_timer_get_time();

    printf("decode: %lld ms, display: %lld ms\n", (t1 - t0) / 1000, (t2 - t1) / 1000);

    printf("result: %s\n", esp_err_to_name(err));

    printf("JPEG size: %d bytes\n", (int)(test_jpg_end - test_jpg_start));
}
