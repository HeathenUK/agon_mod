# ----------------------------
# Makefile Options
# ----------------------------

NAME = MOD
DESCRIPTION = "Agon .MOD tracker"
COMPRESSED = NO
LDHAS_EXIT_HANDLER:=0

CFLAGS = -Wall -Wextra -Oz
CXXFLAGS = -Wall -Wextra -Oz

# ----------------------------

include $(shell cedev-config --makefile)
