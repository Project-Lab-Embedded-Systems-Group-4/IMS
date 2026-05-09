IDF = idf.py

BUILD_DIR = build
APP = hello_world

.PHONY: build clean size-elf size-files size-components

build:
	${IDF} build

clean:
	${IDF} clean

flash:
	${IDF} flash 

size-elf:
	${IDF} size

size-files:
	${IDF} size-files

size-components:
	${IDF} size-components
