.PHONY: all check clean
.SUFFIXES : .c .o

CPPFLAGS	:= -I../../include
LDFLAGS		:= -L../..
LDLIBS		:= -lmpi

ifneq (,$(findstring osf, $(CONFIG)))
LDFLAGS		+= -rpath ../..
endif

TARGET		:= ulmtest
MPIRUN		:= ../../mpirun

SRC =	ulmtest.c \
	test_env.c \
	test_sping.c \
	test_dping.c \
	test_wildproc.c \
	test_wildtag.c \
	test_wildall.c \
	test_bcast.c \
	test_mcast.c \
	prand.c \
	util.c
OBJ = $(addsuffix .o, $(basename $(SRC)))

all: $(TARGET)

ulmtest: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

$(OBJ): ulmtest.h

clean:
	rm -f $(TARGET) $(OBJ) core* Cli* ULMdebug* DBG ULM_LOG

check:	all
	@echo ">>>> "
	@echo ">>>> ULM version..."
	@echo ">>>> "
	$(MPIRUN) -version
	@echo ">>>> "
	@echo ">>>> Running tests with data checking OFF..."
	@echo ">>>> "
	$(MPIRUN) -nolsf -np 4 -exe ulmtest -r 1k -b 1k,1M sping dping wildproc wildtag
	@echo ">>>> "
	@echo ">>>> Running tests with data checking ON..."
	@echo ">>>> "
	$(MPIRUN) -nolsf -np 4 -exe ulmtest -c -r 1k -b 1k,1M sping dping wildproc wildtag
