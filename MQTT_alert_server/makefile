# variables
INCLUDES =
SOURCES = main.c
OBJFILES = main.o
EXEC = prog.bin
#Compilation parameters
CP = -Wall -Wextra
LIBS = -lpaho-mqtt3cs
# regras de sufixo
.SUFFIXES : .c .o
# transform .c into .o ; $< -- filename
.c.o:
	gcc -g ${CP} -c $<
# main program
${EXEC}: ${OBJFILES}
	gcc -g ${CP} -o ${EXEC} ${OBJFILES} ${LIBS}
# run the program
run: ${EXEC}
	./${EXEC}
#keep only the source files
clean:
	rm ${OBJFILES} ${EXEC}
