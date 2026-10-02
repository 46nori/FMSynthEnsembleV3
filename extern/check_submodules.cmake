# extern/ の各 submodule が extern/submodules.lock のコミットと一致するか検査する。
# 未初期化・バージョン違い・lock への登録漏れは Configure を止める。root CMakeLists.txt が
# FREERTOS_KERNEL_PATH を決めたあとに include する。

find_package(Git QUIET)
if(NOT GIT_FOUND)
    message(WARNING "git not found; skipping the extern/submodules.lock check.")
    return()
endif()

get_filename_component(_repo_root "${CMAKE_CURRENT_LIST_DIR}/.." REALPATH)
get_filename_component(_freertos_in_use "${FREERTOS_KERNEL_PATH}" REALPATH)

set(_locked_paths "")
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/submodules.lock" _lock_lines REGEX "^[^#]")
foreach(_line IN LISTS _lock_lines)
    if(NOT _line MATCHES "^([^ \t]+)[ \t]+([0-9a-f]+)")
        continue()
    endif()
    set(_path "${CMAKE_MATCH_1}")
    set(_want "${CMAKE_MATCH_2}")
    list(APPEND _locked_paths "${_path}")
    get_filename_component(_dir "${_repo_root}/${_path}" REALPATH)

    # FREERTOS_KERNEL_PATH で別の場所を指定したときは、使わない submodule を検査しない。
    # 以前の Configure でキャッシュされたパスが残っている場合もここに来るので警告にする
    if(_path STREQUAL "extern/FreeRTOS-Kernel" AND NOT _freertos_in_use STREQUAL _dir)
        message(WARNING
            "FreeRTOS-Kernel is taken from '${FREERTOS_KERNEL_PATH}' (FREERTOS_KERNEL_PATH), "
            "not from the submodule, so its version is not checked against extern/submodules.lock. "
            "If this is unintended, delete the build directory and configure again.")
        continue()
    endif()

    # .git が無いディレクトリで rev-parse すると親リポジトリの HEAD を返すので先に確認する
    if(NOT EXISTS "${_dir}/.git")
        # 取得方法は、親リポジトリがこの submodule のコミットを記録しているかで変わる
        set(_registered "")
        if(EXISTS "${_repo_root}/.git")
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" -C "${_repo_root}" ls-files -s -- "${_path}"
                OUTPUT_VARIABLE _registered
                ERROR_QUIET)
        endif()
        if(_registered MATCHES "^160000 ")
            set(_how "Run 'git submodule update --init --recursive'.")
        elseif(EXISTS "${_repo_root}/.git")
            string(CONCAT _how
                "This repository does not record the submodule commits (it was re-created with "
                "'git init'), so 'git submodule update' cannot fetch them. "
                "Run 'bash scripts/restore-submodules.sh'.")
        else()
            string(CONCAT _how
                "This source tree is not a git repository (for example a ZIP from GitHub, which "
                "does not contain submodules). Run 'git init', then "
                "'bash scripts/restore-submodules.sh'.")
        endif()

        file(GLOB _entries LIST_DIRECTORIES true "${_dir}/*")
        if(_entries)
            message(FATAL_ERROR
                "Submodule '${_path}' contains files but no git metadata ('.git'), so the build "
                "cannot tell which version these files are.\n"
                "Remove the directory and fetch it again. ${_how}")
        endif()
        message(FATAL_ERROR
            "Submodule '${_path}' has not been fetched (the directory is empty or missing).\n"
            "${_how}")
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${_dir}" rev-parse HEAD
        OUTPUT_VARIABLE _have
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_VARIABLE _err
        ERROR_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _rc)
    if(_rc)
        message(FATAL_ERROR "Could not read the commit of submodule '${_path}': ${_err}")
    endif()
    if(NOT _have STREQUAL _want)
        message(FATAL_ERROR
            "Submodule '${_path}' is at commit '${_have}', but extern/submodules.lock pins it to "
            "'${_want}'.\n"
            "The build stops so that every environment uses the same library versions.\n"
            "  - To use the pinned version: run 'git submodule update --init --recursive'.\n"
            "  - To change the version on purpose: update the line for '${_path}' in "
            "extern/submodules.lock to the new commit.")
    endif()
endforeach()

# lock に載っていない submodule は git init し直すとバージョンが失われるので、登録漏れも止める
file(STRINGS "${_repo_root}/.gitmodules" _module_lines REGEX "^[ \t]*path[ \t]*=")
foreach(_line IN LISTS _module_lines)
    string(REGEX REPLACE "^[ \t]*path[ \t]*=[ \t]*" "" _path "${_line}")
    string(STRIP "${_path}" _path)
    if(NOT _path IN_LIST _locked_paths)
        message(FATAL_ERROR
            "Submodule '${_path}' is listed in .gitmodules but not in extern/submodules.lock.\n"
            "Without a lock entry its version would be lost if the repository is re-created "
            "with 'git init'. Add a line with its path and commit "
            "('git -C ${_path} rev-parse HEAD').")
    endif()
endforeach()
