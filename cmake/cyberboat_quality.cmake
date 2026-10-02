function(cyberboat_apply_c_quality target)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Werror
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wstrict-prototypes
        -Wmissing-prototypes
    )
endfunction()