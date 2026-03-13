// Защита от повторного включения заголовка.
#pragma once
// Подключаем optional.
#include <optional>
// Подключаем string.
#include <string>
// Подключаем unordered_map.
#include <unordered_map>

// Пространство имен модуля.
namespace am {

// Назначение блока.
// IConfigManager: хранит данные и связанную логику.
// Единый контракт для расширения модуля.
// Продолжение общей логики.
class IConfigManager {
// Публичный API.
public:
// ~IConfigManager: ключевой шаг текущего сценария.
// Путь данных: вход -> проверки -> результат.
// Значение сохраняется для следующих шагов.
    virtual ~IConfigManager() = default;

// load: ключевой шаг текущего сценария.
// Путь данных: вход -> проверки -> результат.
// Значение сохраняется для следующих шагов.
    virtual void load() = 0;

// get_string: ключевой шаг текущего сценария.
// Путь данных: вход -> проверки -> результат.
// Значение сохраняется для следующих шагов.
    virtual std::optional<std::string> get_string(const std::string& key) const = 0;
// get_double: ключевой шаг текущего сценария.
// Путь данных: вход -> проверки -> результат.
// Значение сохраняется для следующих шагов.
    virtual std::optional<double> get_double(const std::string& key) const = 0;
// get_bool: ключевой шаг текущего сценария.
// Путь данных: вход -> проверки -> результат.
// Значение сохраняется для следующих шагов.
    virtual std::optional<bool> get_bool(const std::string& key) const = 0;
// Конец объявления типа.
};

// ConfigManager: хранит данные и связанную логику.
// Единый контракт для расширения модуля.
// Продолжение общей логики.
class ConfigManager final : public IConfigManager {
// Публичный API.
public:
    // Пояснение к следующему блоку.
// ConfigManager: ключевой шаг текущего сценария.
// Путь данных: вход -> проверки -> результат.
// Подготовка к следующему шагу.
    explicit ConfigManager(std::string path);

// load: ключевой шаг текущего сценария.
// Путь данных: вход -> проверки -> результат.
// Подготовка к следующему шагу.
    void load() override;

// get_string: ключевой шаг текущего сценария.
// Путь данных: вход -> проверки -> результат.
// Подготовка к следующему шагу.
    std::optional<std::string> get_string(const std::string& key) const override;
// get_double: ключевой шаг текущего сценария.
// Путь данных: вход -> проверки -> результат.
// Подготовка к следующему шагу.
    std::optional<double> get_double(const std::string& key) const override;
// get_bool: ключевой шаг текущего сценария.
// Путь данных: вход -> проверки -> результат.
// Подготовка к следующему шагу.
    std::optional<bool> get_bool(const std::string& key) const override;

// Внутреннее состояние.
private:
// Подготовка к следующему шагу.
    std::string path_;
// Подготовка к следующему шагу.
    std::unordered_map<std::string, std::string> kv_;
// Конец объявления типа.
};

// Технический шаг.
} // закрываем пространство имен am


