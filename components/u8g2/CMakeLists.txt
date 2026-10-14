file(GLOB U8G2_SRCS "*.c")
idf_component_register(SRCS ${U8G2_SRCS}
                       INCLUDE_DIRS ".")
