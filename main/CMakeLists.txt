file(GLOB_RECURSE SRC_FILES *.*)

idf_component_register(SRCS main.c ${SRC_FILES}
        INCLUDE_DIRS ".")