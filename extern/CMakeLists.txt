# External libraries (git submodules - do not modify directly)
add_subdirectory(NJU72343-library)
if(BUILD_SD_CARD)
    add_subdirectory(no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src ${CMAKE_BINARY_DIR}/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico)
endif()

# LcdMenu (Arduinoライブラリ、CMakeLists.txtを持たないため自前でadd_libraryする)
# doc/design_lcd_menu.md 3章・3.1節参照
if(BUILD_I2C_DISPLAY)
    add_library(lcdmenu STATIC
        LcdMenu/src/LcdMenu.cpp
        LcdMenu/src/MenuItem.cpp
        LcdMenu/src/MenuScreen.cpp
        LcdMenu/src/renderer/MenuRenderer.cpp
        LcdMenu/src/renderer/CharacterDisplayRenderer.cpp
    )
    target_include_directories(lcdmenu PUBLIC LcdMenu/src)
    # Arduino互換シム(byte型・millis()等)はsrc/platform/arduino_compatに置く。
    # platformは「externの部品をこの基板向けに橋渡しする」役割のため、extern自体には
    # 置かない(extern/ 内はsubmoduleのみ、直接編集しない原則)。
    # LcdMenuの公開ヘッダ(MenuRenderer.h等)からもincludeされるため、lcdmenuを
    # リンクする側にも伝播するPUBLICにする
    target_include_directories(lcdmenu PUBLIC ${CMAKE_SOURCE_DIR}/src/platform/arduino_compat)
    target_link_libraries(lcdmenu PUBLIC pico_stdlib)
endif()
