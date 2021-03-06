#
# Three dependency modes
# 1. using system libraries
# 2. using built-in libraries (The easiest way on windows)
# 3. using libraries built from source
#
set(DEPENDENCY_MODE 2)

#
# Linking mode is used by dependency mode 1
#
set(LINKING_MODE static) # static/shared

#
# CODE
#
project(chest)
set(CMAKE_C_STANDARD 90)
set(CMAKE_CXX_STANDARD 14)

add_executable(chest
        main.c
        enc.c
        cmd.c
        gui/gui.cpp)

set(DEPENDENCIES_DIR ${CMAKE_SOURCE_DIR}/dependencies)

if(DEPENDENCY_MODE EQUAL 1)
    find_library(fltk libfltk.a REQUIRED)
    find_library(fltk_images libfltk_images.a REQUIRED)
    find_library(sodium libsodium.a REQUIRED)

    if(LINKING_MODE STREQUAL static)
        # get link flags for fltk
        execute_process(COMMAND fltk-config --ldstaticflags
                OUTPUT_VARIABLE ld_static_flags
                OUTPUT_STRIP_TRAILING_WHITESPACE
                COMMAND_ERROR_IS_FATAL ANY)
        target_link_libraries(chest ${fltk} ${fltk_images} ${sodium}
                ${ld_static_flags}
                -static-libstdc++ -static-libgcc)
    else()
        target_link_libraries(chest fltk fltk_images sodium)
    endif()
elseif(DEPENDENCY_MODE EQUAL 2)
    set(BINARIES_DIR ${DEPENDENCIES_DIR}/from_shipped_binaries)
    if (WIN32)
        set(PREFIX ${BINARIES_DIR}/mingw)
        target_include_directories(chest PRIVATE ${PREFIX}/include)
        target_link_libraries(chest ${PREFIX}/lib/libfltk.a ${PREFIX}/lib/libfltk_images.a ${PREFIX}/lib/libsodium.a
                -mwindows -pipe -Wl,--enable-auto-import -Wl, --enable-runtime-pseudo-reloc ${PREFIX}/lib/libfltk.a -lole32 -luuid -lcomctl32
                -static)
    else()
        set(PREFIX ${BINARIES_DIR}/linux)
        target_include_directories(chest PRIVATE ${PREFIX}/include)
        target_link_libraries(chest ${PREFIX}/lib/libfltk.a ${PREFIX}/lib/libfltk_images.a ${PREFIX}/lib/libsodium.a
                -Wl,-z,relro -Wl,-z,now -Wl,--as-needed ${PREFIX}/lib/libfltk.a -lXrender -lXfixes -lXext -lXft -lfontconfig -lXinerama -lpthread -ldl -lm -lX11
                -static-libstdc++ -static-libgcc)
    endif()
else()
    set(SRC_DIR " ")
    set(DOWNLOAD_DIR " ")
    function(my_download_extract url expected_hash)
        get_filename_component(filename ${url} NAME)
        set(file_path ${DOWNLOAD_DIR}/${filename})
        set(file_hash " ")
        if(EXISTS ${file_path})
            file(MD5 ${file_path} file_hash)
        endif()
        if(NOT EXISTS ${file_path} OR ${expected_hash} STREQUAL ${file_hash})
            message("downloading ${filename} from ${url}")
            file(DOWNLOAD ${url}
                    ${file_path}
                    SHOW_PROGRESS
                    EXPECTED_HASH ${expected_hash})
            message("extracting ${filename} to ${file_path}")
            file(ARCHIVE_EXTRACT INPUT
                    ${file_path}
                    DESTINATION ${SRC_DIR})
        endif()
    endfunction()

    set(MY_EXEC_PROCESS_OPTS " ")
    function(my_exec_process working_directory command)
        execute_process(COMMAND ${command} ${MY_EXEC_PROCESS_OPTS}
                WORKING_DIRECTORY ${working_directory}
                COMMAND_ERROR_IS_FATAL ANY)
    endfunction()

    set(INSTALL_PREFIX " ")
    set(MY_CONFIGURE_MAKE_OPTS " ")
    function(my_configure_make src_dir)
        set(MY_EXEC_PROCESS_OPTS ${MY_CONFIGURE_MAKE_OPTS} --prefix ${INSTALL_PREFIX})
        my_exec_process(${src_dir} ./configure)
        set(MY_EXEC_PROCESS_OPTS install)
        my_exec_process(${src_dir} make)
    endfunction()

    set(MY_CMAKE_MAKE_OPTS " ")
    function(my_cmake_make src_dir)
        set(MY_EXEC_PROCESS_OPTS . -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} ${MY_CMAKE_MAKE_OPTS})
        my_exec_process(${src_dir} cmake)
        set(MY_EXEC_PROCESS_OPTS install)
        my_exec_process(${src_dir} make)
    endfunction()

    cmake_minimum_required(VERSION 3.18)

    set(DIR ${DEPENDENCIES_DIR}/from_source)
    file(MAKE_DIRECTORY ${DIR})

    set(SRC_DIR ${DIR}/sources)
    set(DOWNLOAD_DIR ${DIR}/downloads)
    file(MAKE_DIRECTORY ${DIR} ${SRC_DIR})

    my_download_extract(
            https://www.fltk.org/pub/fltk/1.3.5/fltk-1.3.5-source.tar.gz
            MD5=e85017defd5a03ae82e634311db87bbf)

    my_download_extract(
            https://download.libsodium.org/libsodium/releases/libsodium-1.0.18-stable.tar.gz
            MD5=36e131dea49b1b620d5410084218e5ee)

    set(BINARIES_DIR ${DIR}/binaries)

    set(INSTALL_PREFIX ${BINARIES_DIR})

    # sodium
    file(GLOB SODIUM_SRC_DIR ${SRC_DIR}/libsodium* )
    set(MY_CONFIGURE_MAKE_OPTS --enable-static --disable-shared)
    my_configure_make(${SODIUM_SRC_DIR})

    # fltk
    file(GLOB FLTK_SRC_DIR ${SRC_DIR}/fltk* )
    set(MY_CMAKE_MAKE_OPTS -DOPTION_BUILD_STATIC_LIBS=ON -DOPTION_SHARED_LIBS=OFF -DOPTION_BUILD_EXAMPLES=OFF "-DOPTION_OPTIM=-Os")
    my_cmake_make(${FLTK_SRC_DIR})

    # get link flags for fltk
    execute_process(COMMAND sh ${DEPENDENCIES_DIR}/scripts/grep_static_flags_from_fltk_config.sh ${FLTK_SRC_DIR}/bin/fltk-config
            OUTPUT_VARIABLE ld_static_flags
            OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY)

    message(ld_static_flags: "${ld_static_flags}")
    # statically link against local build
    target_include_directories(chest PRIVATE ${INSTALL_PREFIX}/include)
    target_link_libraries(chest ${INSTALL_PREFIX}/lib/libsodium.a ${INSTALL_PREFIX}/lib/libfltk.a ${INSTALL_PREFIX}/lib/libfltk_images.a
            -lm -lX11 -lXext -lpthread -lXinerama -lXfixes -lXcursor -lXft -lXrender -lfontconfig -ldl
            -static-libstdc++ -static-libgcc)
endif()