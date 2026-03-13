// Защита от повторного включения заголовка.
#pragma once

// Подключаем cctype.
#include <cctype>
// Подключаем string.
#include <string>
// Подключаем utility.
#include <utility>

// Пространство имен модуля.
namespace am::str {

// Пояснение к алгоритму.
// trim_copy: ключевой шаг текущего сценария.
// Путь данных: вход -> проверки -> результат.
// Продолжение общей логики.
inline std::string trim_copy(std::string s) {
// Повторяем, пока условие истинно.
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
// Повторяем, пока условие истинно.
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
// Возвращаем вычисленное значение.
    return s;
// Конец текущего блока.
}

// Пояснение к текущему шагу.
// upper_copy: ключевой шаг текущего сценария.
// Путь данных: вход -> проверки -> результат.
// Продолжение общей логики.
inline std::string upper_copy(std::string s) {
// Обходим все элементы.
    for (char& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
// Возвращаем вычисленное значение.
    return s;
// Конец текущего блока.
}

// Пояснение к текущему шагу.
// upper_trim_copy: ключевой шаг текущего сценария.
// Путь данных: вход -> проверки -> результат.
// Поддерживаем линейную логику.
inline std::string upper_trim_copy(std::string s) {
// Возвращаем вычисленное значение.
    return upper_copy(trim_copy(std::move(s)));
// Конец текущего блока.
}

// Подготовка данных к следующему шагу.
} // закрываем пространство имен am::str


