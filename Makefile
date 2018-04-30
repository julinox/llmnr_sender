#####################################################################
# This will only work in GNU GCC or a C compiler who supports       #
# the -MD -MP flags provided by GCC                                 #
#####################################################################

#####################################################################
# Variables                                                         #
#####################################################################

EXEC := nss
CC := gcc
LIB := libnss_llmnr.so.2.1
SONAME := libnss_llmnr.so.2

LIBFLAG := -shared -o
LFLAGS := -g -Wall -Wextra -o
CFLAGS := -fPIC -c -g -Wall -Wextra

INSTALLFILE := .install_nss.sh

#####################################################################
# Dependency generation variables                                   #
# MMD to generate the dependency file                               #
# MT to set the name of the target                                  #
# MP generates dummy rules for all the prerequisites. This will     #
# prevent make from generate errors from missing prerequistes files #
# MF to indicate where to put the output file                       #
#####################################################################

DEPENDENCYDIR := .d
DEPENDENCYFLAGS = -MMD -MT $@ -MP -MF $(DEPENDENCYDIR)/$*.d
POSTCOMPILE = mv -f $(DEPENDENCYDIR)/$*.td $(DEPENDENCYDIR)/$*.d

#####################################################################
# Variables related to source, header and object files              #
# SRCEXTRA will contain source files that don't have a              #
# header file                                                       #
#####################################################################

SRC := nss_llmnr_s1.c nss_llmnr_s2.c nss_llmnr_s3.c\
       nss_llmnr_cache.c nss_llmnr_netinterface.c \
       nss_llmnr_responses.c nss_llmnr_answers.c \
       nss_llmnr_strlist.c nss_llmnr_utils.c \
       nss_llmnr_print.c nss_llmnr_packet.c \
       nss_llmnr_cacheclean.c

SRCEXXTRA := nss_llmnr.c
INCLUDE := nss_llmnr_defs.h $(SRC:.c=.h)
OBJS := $(SRC:.c=.o) $(SRCEXXTRA:.c=.o)

#####################################################################
# Paths for source and includes directorys                          #
#####################################################################

SRCPATH := src
INCPATH := include
vpath %.c $(SRCPATH)
vpath %.h $(INCPATH)
vpath %.d $(DEPENDENCYDIR)

#####################################################################
# Rules                                                             #
#####################################################################

$(shell mkdir -p $(DEPENDENCYDIR) >/dev/null)

$(LIB): $(OBJS) $(CACHEC)
	$(CC) $(LIBFLAG) $(LIB) -Wl,-soname,$(SONAME) $(OBJS)

%.o: %.c
	$(CC) $(DEPENDENCYFLAGS) $(CFLAGS) $<

%.d: ;
-include $(patsubst %,$(DEPENDENCYDIR)/%.d,$(basename $(SRC)))

install:
	@chmod 755 $(INSTALLFILE)
	@./$(INSTALLFILE)

#####################################################################
# Phony rules                                                       #
#####################################################################

.PHONY: lib clean cleanall dummy

lib: CFLAGS += $(CLIBFLAGS)
lib: $(LIB)

clean:
	@rm -f $(OBJS)

cleanall:
	@rm -f $(OBJS) $(EXEC) $(LIB)

dummy:
	@$(CC) $(CFLAGS) src/nss_llmnr.c
	@$(CC) $(CFLAGS) src/nss_llmnr_s1.c
	@$(CC) $(CFLAGS) src/nss_llmnr_s2.c
	@$(CC) $(CFLAGS) src/nss_llmnr_s3.c
	@$(CC) $(CFLAGS) src/nss_llmnr_cache.c
	@$(CC) $(CFLAGS) src/nss_llmnr_netinterface.c
	@$(CC) $(CFLAGS) src/nss_llmnr_responses.c
	@$(CC) $(CFLAGS) src/nss_llmnr_answers.c
	@$(CC) $(CFLAGS) src/nss_llmnr_strlist.c
	@$(CC) $(CFLAGS) src/nss_llmnr_utils.c
	@$(CC) $(CFLAGS) src/nss_llmnr_print.c
	@$(CC) $(CFLAGS) src/nss_llmnr_packet.c
	@$(CC) $(CFLAGS) src/nss_llmnr_cacheclean.c
	@$(CC) $(LIBFLAG) $(LIB) -Wl,-soname,$(SONAME) $(OBJS)

