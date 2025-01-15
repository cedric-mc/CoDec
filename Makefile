# Variables générales
CC  = gcc
STD = -std=c17

PFLAGS = $(incG2X) -I./include
CFLAGS = -Wpointer-arith -Wall

# Optimisation ou débogage
ifeq ($(GDB),1)
  CFLAGS += -g
  EXT    = .gdb
else
  CFLAGS += -O2
  EXT    =
endif

# Sélection de la bonne version de libg2x*.so
LFLAGS = $(libG2X)$(EXT)

# Dossier source
SRC = src/

# Liste des exécutables à générer
ALL = main

# Règle spécifique pour l'exécutable 'main'
main: difimg.o main.o
	$(CC) $^ $(LFLAGS) -o $@

# Règle générique pour créer un fichier .o à partir d'un fichier .c
%.o : $(SRC)%.c
	@echo "Compilation de $@"
	@$(CC) $(STD) $(CFLAGS) $(PFLAGS) -c $< -o $@
	@echo "------------------------"

# Règle générique pour assembler un exécutable à partir d'un fichier .o
% : %.o
	@echo "Assemblage [$^] -> $@"
	@$(CC) $^ $(LFLAGS) -o $@
	@echo "------------------------"

# Règles spécifiques pour les exécutables pgmtodif et diftopgm
pgmtodif: pgmtodif.o difimg.o
	$(CC) -o pgmtodif pgmtodif.o difimg.o -lg2x

diftopgm: diftopgm.o difimg.o
	$(CC) -o diftopgm diftopgm.o difimg.o -lg2x

pgmtodif.o: pgmtodif.c difimg.h
	$(CC) -c pgmtodif.c

diftopgm.o: diftopgm.c difimg.h
	$(CC) -c diftopgm.c

difimg.o: difimg.c difimg.h
	$(CC) -c difimg.c

# Génération de tous les exécutables listés dans ALL
all : $(ALL)

# Règles de nettoyage
.PHONY : clean cleanall ?

clean :
	@rm -f *.o

cleanall :
	@rm -f *.o $(ALL)

# Informations sur les paramètres de compilation
? :
	@echo "--------- Compilation Informations ---------"
	@echo "  Compilateur : $(CC)"
	@echo "  Standard    : $(STD)"
	@echo "  PFLAGS      : $(PFLAGS)"
	@echo "  CFLAGS      : $(CFLAGS)"
	@echo "  LFLAGS      : $(LFLAGS)"
