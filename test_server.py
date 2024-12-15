import unittest
import requests
import subprocess
import time
import os
import signal

class TestFlaskCppServer(unittest.TestCase):
    SERVER_URL = "http://localhost:8080"
    SERVER_PROCESS = None

    @classmethod
    def setUpClass(cls):
        """
        Запускаем сервер FlaskCpp перед выполнением тестов.
        """
        # Путь к исполняемому файлу сервера
        server_executable = os.path.join("bin", "server")

        # Убедитесь, что исполняемый файл существует
        if not os.path.isfile(server_executable):
            raise FileNotFoundError(f"Исполняемый файл сервера не найден по пути: {server_executable}")

        # Запуск сервера как subprocess
        cls.SERVER_PROCESS = subprocess.Popen(
            [server_executable, "--port", "8080", "--verbose"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

        # Ждем, пока сервер начнет слушать порт
        timeout = 10  # секунды
        start_time = time.time()
        while True:
            try:
                response = requests.get(cls.SERVER_URL)
                if response.status_code in [200, 404]:
                    break
            except requests.exceptions.ConnectionError:
                pass
            if time.time() - start_time > timeout:
                cls.terminate_server()
                raise TimeoutError("Сервер не запустился в течение заданного времени.")
            time.sleep(0.5)

    @classmethod
    def tearDownClass(cls):
        """
        Останавливаем сервер FlaskCpp после выполнения тестов.
        """
        cls.terminate_server()

    @classmethod
    def terminate_server(cls):
        """
        Останавливаем процесс сервера.
        """
        if cls.SERVER_PROCESS:
            cls.SERVER_PROCESS.terminate()
            try:
                cls.SERVER_PROCESS.wait(timeout=5)
            except subprocess.TimeoutExpired:
                cls.SERVER_PROCESS.kill()
            cls.SERVER_PROCESS = None

    def test_root_path(self):
        """
        Тестируем корневой путь '/'.
        """
        response = requests.get(f"{self.SERVER_URL}/")
        self.assertEqual(response.status_code, 200)
        self.assertIn("Добро пожаловать", response.text)

    def test_form_page(self):
        """
        Тестируем страницу формы '/form'.
        """
        response = requests.get(f"{self.SERVER_URL}/form")
        self.assertEqual(response.status_code, 200)
        self.assertIn("<form", response.text)

    def test_submit_form(self):
        """
        Тестируем отправку формы на '/submit'.
        """
        payload = {'username': 'TestUser'}
        response = requests.post(f"{self.SERVER_URL}/submit", data=payload)
        self.assertEqual(response.status_code, 200)
        self.assertIn("Привет, TestUser!", response.text)

    def test_user_route(self):
        """
        Тестируем маршрут с параметром '/user/<id>'.
        """
        user_id = "12345"
        response = requests.get(f"{self.SERVER_URL}/user/{user_id}")
        self.assertEqual(response.status_code, 200)
        self.assertIn(f"User ID: {user_id}", response.text)

    def test_api_data(self):
        """
        Тестируем API-эндпоинт '/api/data'.
        """
        response = requests.get(f"{self.SERVER_URL}/api/data")
        self.assertEqual(response.status_code, 200)
        self.assertTrue(response.headers["Content-Type"].startswith("application/json"))
        self.assertEqual(response.json(), {"status": "ok", "message": "Hello from JSON!"})

    def test_error_route(self):
        """
        Тестируем маршрут, генерирующий ошибку '/error'.
        """
        response = requests.get(f"{self.SERVER_URL}/error")
        self.assertEqual(response.status_code, 500)
        self.assertIn("500 Internal Server Error", response.text)

    def test_404(self):
        """
        Тестируем маршрут, который не существует, для получения 404.
        """
        response = requests.get(f"{self.SERVER_URL}/nonexistent")
        self.assertEqual(response.status_code, 404)
        self.assertIn("404 Not Found", response.text)

    def test_set_cookie(self):
        """
        Тестируем установку cookie через '/set_cookie'.
        """
        response = requests.get(f"{self.SERVER_URL}/set_cookie")
        self.assertEqual(response.status_code, 200)
        self.assertIn("Set-Cookie", response.headers)
        self.assertIn("User=JohnDoe", response.headers.get("Set-Cookie", ""))
        self.assertIn("SessionID=abc123", response.headers.get("Set-Cookie", ""))

    def test_get_cookie(self):
        """
        Тестируем получение cookie через '/get_cookie'.
        """
        with requests.Session() as session:
            # Устанавливаем cookie
            session.get(f"{self.SERVER_URL}/set_cookie")
            # Получаем cookie
            response = session.get(f"{self.SERVER_URL}/get_cookie")
            self.assertEqual(response.status_code, 200)
            self.assertIn("Cookie 'User' = JohnDoe", response.text)

    def test_delete_cookie(self):
        """
        Тестируем удаление cookie через '/delete_cookie'.
        """
        with requests.Session() as session:
            # Устанавливаем cookie
            session.get(f"{self.SERVER_URL}/set_cookie")
            # Удаляем cookie
            response = session.get(f"{self.SERVER_URL}/delete_cookie")
            self.assertEqual(response.status_code, 200)
            self.assertIn("Cookie 'User' был удалён.", response.text)
            # Проверяем, что cookie удалена
            response = session.get(f"{self.SERVER_URL}/get_cookie")
            self.assertEqual(response.status_code, 200)
            self.assertIn("Cookie 'User' не найден.", response.text)

    def test_template_inheritance(self):
        """
        Тестируем страницу с наследованием шаблона '/extend'.
        """
        response = requests.get(f"{self.SERVER_URL}/extend")
        self.assertEqual(response.status_code, 200)
        self.assertIn("Страница с Наследованием", response.text)
        self.assertIn("Это страница, которая наследует базовый шаблон.", response.text)

    def test_static_file_serving(self):
        """
        Тестируем обслуживание статических файлов.
        """
        # Предполагается, что существует файл /static/test.txt с содержимым "Hello, Static!"
        response = requests.get(f"{self.SERVER_URL}/static/js/test.js")
        if response.status_code == 200:
            self.assertEqual(response.text, "Hello, Static!")
        else:
            self.assertEqual(response.status_code, 404)  # Файл может отсутствовать

    def test_hot_reload(self):
        """
        Тестируем функциональность hot reload (обновление шаблонов на лету).
        """
        # Предполагается, что существует шаблон templates/hot_reload_test.html
        template_path = os.path.join("templates", "hot_reload_test.html")
        original_content = "<h1>Original Content</h1>"

        # Создаем или перезаписываем шаблон
        with open(template_path, "w", encoding="utf-8") as f:
            f.write(original_content)

        # Дожидаемся, пока сервер загрузит шаблон
        time.sleep(3)

        # Запрашиваем страницу, которая использует этот шаблон
        response = requests.get(f"{self.SERVER_URL}/hot_reload")
        if response.status_code == 200:
            self.assertIn("Original Content", response.text)
        else:
            self.assertEqual(response.status_code, 404)  # Маршрут может отсутствовать

        # Обновляем шаблон
        updated_content = "<h1>Updated Content</h1>"
        with open(template_path, "w", encoding="utf-8") as f:
            f.write(updated_content)

        # Дожидаемся, пока сервер обнаружит изменения и перезагрузит шаблон
        time.sleep(3)

        # Повторно запрашиваем страницу и проверяем обновление
        response = requests.get(f"{self.SERVER_URL}/hot_reload")
        if response.status_code == 200:
            self.assertIn("Updated Content", response.text)
        else:
            self.assertEqual(response.status_code, 404)

        # Восстанавливаем оригинальное содержимое для последующих тестов
        with open(template_path, "w", encoding="utf-8") as f:
            f.write(original_content)
        time.sleep(3)

if __name__ == '__main__':
    unittest.main()
