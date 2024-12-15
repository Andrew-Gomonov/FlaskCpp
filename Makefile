# Компилятор и флаги компиляции
CXX = g++
CXXFLAGS = -std=c++17 -pthread -fPIC -I./src/headers

# Опциональные флаги
# Если ENABLE_PHP установлено, добавляем флаг -DENABLE_PHP
ifeq ($(ENABLE_PHP),1)
    CXXFLAGS += -DENABLE_PHP
endif

# Директории
SRC_DIR = src
BIN_DIR = bin
LIB_DIR = lib

# Источники и объектные файлы для библиотеки
LIB_SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
LIB_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp, $(BIN_DIR)/%.o, $(LIB_SOURCES))

# Библиотечные цели
STATIC_LIB = $(LIB_DIR)/libFlaskCpp.a
SHARED_LIB = $(LIB_DIR)/libFlaskCpp.so

# Исходный файл и объектный файл для исполняемого файла
MAIN_SOURCE = main.cpp
MAIN_OBJECT = $(BIN_DIR)/main.o

# Целевой исполняемый файл
TARGET = $(BIN_DIR)/server

# Файл тестов
TEST_SCRIPT = test_server.py

# Цели по умолчанию
all: $(TARGET) $(STATIC_LIB) $(SHARED_LIB) move_server test

# Линковка исполняемого файла с библиотекой
$(TARGET): $(MAIN_OBJECT) $(STATIC_LIB) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(MAIN_OBJECT) -L$(LIB_DIR) -lFlaskCpp -o $(TARGET)
	@echo "Исполняемый файл создан: $(TARGET)"

# Компиляция main.cpp в объектный файл
$(MAIN_OBJECT): $(MAIN_SOURCE) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -c $(MAIN_SOURCE) -o $(MAIN_OBJECT)
	@echo "Скомпилирован: $(MAIN_SOURCE) -> $(MAIN_OBJECT)"

# Линковка статической библиотеки из объектных файлов
$(STATIC_LIB): $(LIB_OBJECTS) | $(LIB_DIR)
	ar rcs $(STATIC_LIB) $(LIB_OBJECTS)
	@echo "Статическая библиотека создана: $(STATIC_LIB)"

# Линковка динамической библиотеки из объектных файлов
$(SHARED_LIB): $(LIB_OBJECTS) | $(LIB_DIR)
	$(CXX) -shared $(CXXFLAGS) $(LIB_OBJECTS) -o $(SHARED_LIB)
	@echo "Динамическая библиотека создана: $(SHARED_LIB)"

# Компиляция всех исходных файлов библиотеки в объектные файлы
$(BIN_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@
	@echo "Скомпилирован: $< -> $@"

# Создание директории bin, если она не существует
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Создание директории lib, если она не существует
$(LIB_DIR):
	mkdir -p $(LIB_DIR)

# Очистка скомпилированных файлов и директорий
clean:
	rm -rf $(BIN_DIR) $(LIB_DIR) server
	@echo "Очистка завершена. Папки bin и lib удалены, исполняемый файл удален."

# Опциональная цель для установки библиотеки и заголовков в системные директории
install: $(STATIC_LIB) $(SHARED_LIB)
	mkdir -p /usr/local/lib
	mkdir -p /usr/local/include/FlaskCpp
	cp $(STATIC_LIB) /usr/local/lib/
	cp $(SHARED_LIB) /usr/local/lib/
	cp $(SRC_DIR)/headers/*.h /usr/local/include/FlaskCpp/
	ldconfig
	@echo "Библиотеки и заголовки установлены."

# Цель для запуска сервера с hot_reload
run: $(TARGET)
	./$(TARGET) --port 8080 --verbose

# Цель для запуска сервера без hot_reload
run-no-hot-reload: $(TARGET)
	./$(TARGET) --port 8080 --verbose --no-hot-reload

# Цель для сборки с поддержкой PHP
php:
	$(MAKE) clean ENABLE_PHP=1
	$(MAKE) all ENABLE_PHP=1

# Цель для запуска модульных тестов
test: $(TARGET) $(STATIC_LIB) $(SHARED_LIB)
	@echo "Запуск модульных тестов..."
	python3 $(TEST_SCRIPT)

# Цель для копирования исполняемого файла в родительскую директорию
move_server: $(TARGET)
	cp $(TARGET) .
	@echo "Исполняемый файл скопирован в ../server"

.PHONY: all clean install run run-no-hot-reload php test move_server
