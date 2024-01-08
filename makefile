# ----------------------------
# Makefile Options
# ----------------------------

NAME = playmod
DESCRIPTION = "Agon .MOD tracker"
COMPRESSED = NO
INIT_LOC = 0B0000
LDHAS_EXIT_HANDLER:=0

CFLAGS = -Wall -Wextra -Oz
CXXFLAGS = -Wall -Wextra -Oz

# ----------------------------

include $(shell cedev-config --makefile)
