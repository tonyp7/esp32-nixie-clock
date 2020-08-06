idf_component_register(
    SRCS "list.c" "webapp.c" "main.c" "ws2812.c" "i2c.c" "display.c" "clock.c" "ds3231.c" "http_client.c" "webapp.c" "list.c"
    INCLUDE_DIRS "" "include"
	EMBED_FILES clock.js iro.js clock.css clock.html timezones.json
)
