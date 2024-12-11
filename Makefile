# Makefile для FlaskCpp

# Компилятор и флаги компиляции
CXX = g++
CXXFLAGS = -std=c++17 -pthread -fPIC -I./src/headers

# Директории
SRC_DIR = src
BIN_DIR = bin
LIB_DIR = lib

# Источники и объектные файлы
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp, $(BIN_DIR)/%.o, $(SOURCES))

# Целевой бинарный файл
TARGET = $(BIN_DIR)/server

# Библиотека
STATIC_LIB = $(LIB_DIR)/libFlaskCpp.a
SHARED_LIB = $(LIB_DIR)/libFlaskCpp.so

# Цель по умолчанию: сборка бинарного файла и библиотек
all: $(TARGET) $(STATIC_LIB) $(SHARED_LIB)

# Линковка исполняемого файла
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(TARGET)
	@echo "Бинарный файл создан: $(TARGET)"

# Компиляция каждого .cpp файла в соответствующий .o файл
$(BIN_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@
	@echo "Скомпилирован: $< -> $@"

# Создание директории bin, если она не существует
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Создание статической библиотеки
$(STATIC_LIB): $(OBJECTS) | $(LIB_DIR)
	ar rcs $(STATIC_LIB) $(OBJECTS)
	@echo "Статическая библиотека создана: $(STATIC_LIB)"

# Создание директории lib, если она не существует
$(LIB_DIR):
	mkdir -p $(LIB_DIR)

# Создание динамической библиотеки
$(SHARED_LIB): $(OBJECTS) | $(LIB_DIR)
	$(CXX) -shared $(CXXFLAGS) $(OBJECTS) -o $(SHARED_LIB)
	@echo "Динамическая библиотека создана: $(SHARED_LIB)"

# Очистка скомпилированных файлов
clean:
	rm -rf $(BIN_DIR) $(LIB_DIR)
	@echo "Очистка завершена. Папки bin и lib удалены."

# Опциональная цель для установки библиотеки и заголовков
install: $(STATIC_LIB) $(SHARED_LIB)
	mkdir -p /usr/local/lib
	mkdir -p /usr/local/include/FlaskCpp
	cp $(STATIC_LIB) /usr/local/lib/
	cp $(SHARED_LIB) /usr/local/lib/
	cp $(SRC_DIR)/headers/*.h /usr/local/include/FlaskCpp/
	ldconfig
	@echo "Библиотеки и заголовки установлены."

# Цель для запуска сервера
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean install run
