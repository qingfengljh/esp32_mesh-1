
set(COMPONENT_SRCS "router_example.c" "app_httpd.c" "app_spiffs.c")

set(COMPONENT_ADD_INCLUDEDIRS "include")

# requirements can't depend on config
set(COMPONENT_REQUIRES mcommon mconfig mwifi mlink)

register_component()
