# pico-sdk・FreeRTOS・FatFS・TinyUSB のライブラリは INTERFACE ライブラリで、ソースをリンク元
# ターゲットに追加する作りになっている。中間の静的ライブラリ（synth, platform 等）が普通に
# リンクすると、静的ライブラリごとに SDK 一式を重複してコンパイルしてしまう。
#
# fmse_compile_only_interface_deps(<exe>) は <exe> からたどれる静的ライブラリについて、
# INTERFACE ライブラリへのリンクを外し、代わりにそのインクルードパス・コンパイル定義・
# コンパイルオプション（依存先まで含めて評価される）だけを渡す。外した INTERFACE ライブラリは
# <exe> に直接リンクし、ソースは <exe> で 1 回だけコンパイルさせる。submodule の静的ライブラリ
# （nju72343 等）も対象になる。全ターゲットの定義後に呼ぶこと。
#
# $<COMPILE_ONLY:...> は INTERFACE_SOURCES も伝播させてしまうため使えない。

set(_FMSE_USAGE_PROPS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS)

function(fmse_compile_only_interface_deps exe)
    set(_pending "")
    get_target_property(_exe_links ${exe} LINK_LIBRARIES)
    if(_exe_links)
        list(APPEND _pending ${_exe_links})
    endif()

    set(_visited "")
    set(_interface_libs "")
    while(_pending)
        list(POP_FRONT _pending _lib)
        if(NOT TARGET "${_lib}" OR _lib IN_LIST _visited)
            continue()
        endif()
        list(APPEND _visited "${_lib}")
        get_target_property(_type ${_lib} TYPE)
        if(NOT _type STREQUAL "STATIC_LIBRARY")
            continue()
        endif()

        # LINK_LIBRARIES は自身のコンパイル用、INTERFACE_LINK_LIBRARIES はリンク元への伝播用
        foreach(_scope "" "INTERFACE_")
            get_target_property(_items ${_lib} ${_scope}LINK_LIBRARIES)
            if(NOT _items)
                continue()
            endif()
            set(_kept "")
            foreach(_item IN LISTS _items)
                # PRIVATE リンクは INTERFACE_LINK_LIBRARIES に $<LINK_ONLY:x> として入る
                set(_name "${_item}")
                if(_item MATCHES "^\\$<LINK_ONLY:([^>]+)>$")
                    set(_name "${CMAKE_MATCH_1}")
                endif()
                if(TARGET "${_name}")
                    get_target_property(_item_type ${_name} TYPE)
                    if(_item_type STREQUAL "INTERFACE_LIBRARY")
                        if(NOT _item STREQUAL _name)
                            continue()
                        endif()
                        foreach(_prop IN LISTS _FMSE_USAGE_PROPS)
                            set_property(TARGET ${_lib} APPEND PROPERTY ${_scope}${_prop}
                                "$<TARGET_PROPERTY:${_name},INTERFACE_${_prop}>")
                        endforeach()
                        # pico-sdk のヘッダは SYSTEM 指定（-isystem）なので、それも引き継ぐ
                        if(_scope STREQUAL "")
                            set(_visibility PRIVATE)
                        else()
                            set(_visibility INTERFACE)
                        endif()
                        target_include_directories(${_lib} SYSTEM ${_visibility}
                            "$<TARGET_PROPERTY:${_name},INTERFACE_SYSTEM_INCLUDE_DIRECTORIES>")
                        list(APPEND _interface_libs "${_name}")
                        continue()
                    endif()
                    list(APPEND _pending "${_name}")
                endif()
                list(APPEND _kept "${_item}")
            endforeach()
            set_property(TARGET ${_lib} PROPERTY ${_scope}LINK_LIBRARIES "${_kept}")
        endforeach()
    endwhile()

    list(REMOVE_DUPLICATES _interface_libs)
    # 実行ファイルは他の target_link_libraries 呼び出しと同じくキーワードなし形式でリンクする
    target_link_libraries(${exe} ${_interface_libs})
endfunction()
