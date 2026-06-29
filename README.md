# Priority HTTP Server

Многопоточный HTTP-сервер на C++17 с приоритетной очередью задач, асинхронным I/O и гибкой конфигурацией.

## Возможности

- Асинхронный ввод-вывод на Boost. Asio/Beast.
- Приоритетная очередь задач с механизмом старения (aging)
- Keep-Alive (HTTP/1.1) с ограничением числа запросов
- Ограничение размера тела запроса, очереди задач, количества соединений
- Отдача статических файлов с защитой от Path Traversal
- Graceful Shutdown с атомарным счётчиком активных сессий
- Асинхронный потокобезопасный логгер
- Конфигурация через JSON-файл и аргументы командной строки
- Автосоздание конфигурационного файла со значениями по умолчанию
- Поддержка CORS
- Юнит-тесты (Google Test) и интеграционные тесты
- Нагрузочное тестирование (wrk) — 154k RPS на /status.

## Технологии

- C++17
- Boost (Asio, Beast, algorithmic)
- nlohmann/json
- CMake 4.0
- Google Test

## Сборка и запуск

### Требования

Компилятор с поддержкой C++17 (Clang, GIC, MSVC)
- CMake 3.16+
- Boost 1.89+ (рекомендуется, но сервер также совместим с более ранними версиями благодаря условной линковке в CMake)

### Сборка

```bash
git clone https://github.com/PanishDeveloper/priority-http-server.git
cd priority-http-server
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Запуск

```bash
./build/prserver
```

Сервер запустится на порту 8080. При первом запуске будет создан файл `config.json` с настройками по умолчанию.

## Конфигурация

Сервер поддерживает настройку через JSON-файл и аргументы командной строки.

### файл `config.json` (основные параметры)

```json
{
  "port": 8080,
  "bind_address": "0.0.0.0",
  "threads": 12,
  "io_threads": 4,
  "static_root": "static",
  "log_level": "INFO",
  "body_limit_mb": 10,
  "max_queue_size": 1000,
  "max_connections": 10000,
  "server_name": "PriorityHttpServer/2.0",
  "sample_rate": 100,
  "enable_keepalive": true,
  "keepalive_timeout_sec": 30,
  "max_keepalive_requests": 500,
  "drain_timeout_sec": 5,
  "static_max_file_size_mb": 10,
  "cors_allow_origin": "*"
}
```

### Аргументы командной строки

 Все параметры можно переопределить через флаги, которые имеют высший приоритет.

```bash
./build/prserver --port 9090 --threads 16 --log-level DEBUG
```

| Флаг                       | Тип              | По умолчанию             | Описание                                                          |
|----------------------------|------------------|--------------------------|-------------------------------------------------------------------|
| `-p, `--port`              | `unsigned short` | `8080`                   | Порт для входящих соединений                                      |
| `-b, `--bind-address`      | `string`         | `0.0.0.0`                | IP-адрес для привязки                                             |
| `-t, `--threads`           | `size_t`         | CPU cores                | Потоки вычислительного пула                                       |
| `-i, `--io-threads`        | `unsigned int`   | `4`                      | Потоки для сетевого I/O                                           |
| `-s, `--static-root`       | `string`         | `static`                 | Папка со статическими файлами                                     |
| `-l, `--log-level`         | `string`         | `INFO`                   | Уровень логирования (DEBUG, INFO, WARNING, ERROR)                 |
| `--log-file`               | `string`         | (пусто)                  | Путь к файлу лога (пусто = консоль)                               |
| `--body-limit`             | `size_t`         | `10`                     | Максимальный размер тела POST-запроса (МБ)                        |
| `--max-queue-size`         | `size_t`         | `1000`                   | Максимальный размер очереди задач                                 |
| `--max-connections`        | `size_t`         | `10000`                  | Максимальное количество одновременных TCP-соединений              |
| `--server-name`            | `string`         | `PriorityHttpServer/2.0` | Значение заголовка `Server`                                       |
| `--sample-rate`            | `unsigned int`   | `100`                    | Логировать каждый N-й запрос с приоритетом                        |
| `--enable-keepalive`       | `bool`           | `true`                   | Глобальное включение/выключение Keep-Alive                        |
| `--keepalive-timeout`      | `seconds`        | `30`                     | Таймаут бездействия Keep-Alive соединения                         |
| `--max-keepalive-requests` | `size_t`         | `500`                    | Максимальное количество запросов в рамках одной Keep-Alive сессии |
| `--drain-timeout`          | `seconds`        | `5`                      | Таймаут ожидания завершения активных сессий при остановке         |
| `--static-max-file-size`   | `size_t`         | `10`                     | Максимальный размер отдаваемого статического файла (МБ)           |
| `--cors-allow-origin`      | `string`         | `*`                      | Значение заголовка `Access-Control-Allow-Origin`                  |

## Маршруты

| Метод   | Путь        | Описание                                   |
|---------|-------------|--------------------------------------------|
| GET     | `/status`   | Состояние сервера (JSON)                   |
| GET     | `/static/*` | Отдача статических файлов                  |
| HEAD    | `/static/*` | Заголовки статических файлов               |
| POST    | `/compute`  | Вычислительная задача (сортировка массива) |

## Тестирование

### Юнит-тесты и интеграционные тесты

```bash
cmake -B cmake-build-tests -DBUILD_TESTS=ON
cmake --build cmake-build-tests
cd cmake-build-tests && ctest --output-on-failure
```

### Нагрузочное тестирование

```bash
wrk -t12 -c500 -d30s http://localhost:8080/status
wrk -t12 -c500 -d30s http://localhost:8080/static/index.html
wrk -t12 -c500 -d30s -s post_compute.lua http://localhost:8080/compute
```

## Производительность

| Эндпоинт                 | RPS      | Задержка |
|--------------------------|----------|----------|
| `GET /status`            | ~154 000 | ~3.2 мс  |
| `GET /static/index.html` | ~44 500  | ~12.4 мс |
| `POST /compute`          | ~120 000 | ~4.3 мс  |
Измерено на MacBook Pro M4Pro (Apple Silicon), Release - сборка

## HTTPS

Для production-развертывания рекомендуется использовать nginx в качестве HTTPS-запросов:

```nginx
server {
    listen 443 ssl;
    server_name localhost;
    ssl_certificate     /path/to/cert.pem;
    ssl_certificate_key /path/to/key.pem;
    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

## Архитектура

- **HttpServer** — оркестратор, управляет жизненным циклом
- **Session** — управление одним HTTP-соединением (Strand для потокобезопасности)
- **RequestProcessor** — выбор стратегии обработки (быстрая / CPU-bound)
- **Router** — маршрутизация запросов к обработчикам
- **ThreadPool** — вычислительный пул с приоритетной очередью и aging
- **AsyncLogger** — асинхронное логирование с фильтрацией по уровням
- **Config** — загрузка конфигурации из JSON и аргументов командной строки

