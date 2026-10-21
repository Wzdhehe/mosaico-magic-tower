idf_component_register(
    SRCS "main.c" "game.c" "game_view.c" "game_battery.c" "game_battery_service.c" "game_device_render.c" "game_preferences.c" "tower.c" "tower_floors.c" "raylib_screen_mirror.c" "game_audio.c" "game_music.c"
    INCLUDE_DIRS "."
    REQUIRES mosaico_game_app mosaico_raylib_fast esp-gsp nvs_flash esp-mosaico-bsp esp_timer esp_driver_i2c)

# GSP needs an image to bind the Raylib framebuffer. This generated placeholder
# is embedded in the application; no external game resource partition is used.
set(canvas_png "${CMAKE_CURRENT_BINARY_DIR}/canvas.png")
execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/make_canvas_png.py"
    "${canvas_png}" RESULT_VARIABLE canvas_result)
if(NOT canvas_result EQUAL 0)
    message(FATAL_ERROR "Could not generate the game canvas placeholder")
endif()
set(CANVAS_IMAGE "${canvas_png}")
configure_file("scene.json.in" "${CMAKE_CURRENT_BINARY_DIR}/scene.json" @ONLY)
gsp_add_bundle(${COMPONENT_LIB}
    SCENES "${CMAKE_CURRENT_BINARY_DIR}/scene.json"
    PIXEL_FORMAT rgb565)

# GSPC 0.6.1 emits escaped Win32 extended paths (\\?\D:\...) in its
# depfile. CMake 4 transforms those into invalid /D:/ paths in Ninja's database.
# This blank game's sole scene has exactly one generated PNG dependency.
# Keep the pinned compiler and real dependency tracking; use equivalent paths
# relative to this build directory for just this application's canvas bundle.
if(CMAKE_HOST_WIN32)
    set(canvas_deps "${CMAKE_CURRENT_BINARY_DIR}/canvas-deps.d.in")
    file(WRITE "${canvas_deps}"
        "gsp_gen_bundle/bundle.gspb: canvas.png scene.json\n")
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gsp_gen_bundle/bundle.gspb"
        APPEND
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${canvas_deps}"
            "${CMAKE_CURRENT_BINARY_DIR}/gsp_gen_bundle/bundle.d")
endif()
