idf_component_register(
    SRCS "main.c" "ws2812.c" "wifi_manager.c" "json.c" "i2c.c" "http_server.c" "dns_server.c" "display.c" "clock.c" "ds3231.c" "http_client.c"
    INCLUDE_DIRS ""
	EMBED_FILES style.css jquery.gz code.js index.html clock.js iro.js clock.css
)
