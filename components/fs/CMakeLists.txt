# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS "nvs_manager.c" "fs.c"       # list the source files of this component
    INCLUDE_DIRS "include"    # optional, add here public include directories
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES      spiffs  json common_defs nvs_flash  # optional, list the public requirements (component names)
    PRIV_REQUIRES       # optional, list the private requirements
)
