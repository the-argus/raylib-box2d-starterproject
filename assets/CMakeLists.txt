include(CMakeRC.cmake)

cmrc_add_resource_library(
  gameresources #
  ALIAS
  game::resources #
  # list asset files here
  missing.png #
)

set_property(TARGET gameresources PROPERTY POSITION_INDEPENDENT_CODE ON)
