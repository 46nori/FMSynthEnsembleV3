# ファームウェアの構造に関するビルドオプション（外部ライブラリ・SDK の構成・特定ハードウェアに
# 依存するコンポーネントの有無）を宣言し、ソースから参照するヘッダ build_config.h を生成する。
#
# fmse_build_option(<NAME> "<説明>" <ON|OFF>) は option() を宣言し、同じ名前のマクロを
# build_config.h に 0/1 で書き出す対象に登録する。オプションを増やすときはこの 1 行だけを書く。
# app/ と synth/ は src/config.h 経由で build_config.h を include する。drivers/ と platform/ は
# アプリケーションの調整値に依存しないよう、build_config.h を直接 include する。
#
# fmse_generate_build_config(<dir>) は登録済みのオプションの値を <dir>/build_config.h に書き出す。
# 内容が変わらない限りファイルを書き換えないので、Configure のたびに再コンパイルは起きない。

set(FMSE_BUILD_OPTIONS "")

macro(fmse_build_option name description default)
    option(${name} "${description}" ${default})
    list(APPEND FMSE_BUILD_OPTIONS ${name})
endmacro()

function(fmse_generate_build_config dir)
    set(_content "// CMake が生成する。編集しない（定義元はルート CMakeLists.txt の fmse_build_option）。\n")
    string(APPEND _content "#pragma once\n\n")
    foreach(_opt IN LISTS FMSE_BUILD_OPTIONS)
        if(${_opt})
            set(_value 1)
        else()
            set(_value 0)
        endif()
        string(APPEND _content "#define ${_opt} ${_value}\n")
        message(STATUS "${_opt}: ${${_opt}}")
    endforeach()
    file(CONFIGURE OUTPUT "${dir}/build_config.h" CONTENT "${_content}")
endfunction()
