#include "words.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <cctype>
#include <algorithm>

int process_file(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) {
        std::cout << "Невозможно открыть файл: " << filename << "\n";
        return 1;
    }

    auto clean = [](std::string s) {
        while (!s.empty() && std::ispunct((unsigned char)s.back()))  s.pop_back();
        while (!s.empty() && std::ispunct((unsigned char)s.front())) s.erase(s.begin());
        return s;
    };

    // lower для ASCII + русских А-Я + Ё в UTF-8
    auto to_lower_ru_utf8 = [](std::string s) {
        for (size_t i = 0; i < s.size(); ) {
            unsigned char c = (unsigned char)s[i];

            if (c < 0x80) { // ASCII
                s[i] = (char)std::tolower(c);
                ++i;
                continue;
            }

            // Русские заглавные (2 байта)
            if (c == 0xD0 && i + 1 < s.size()) {
                unsigned char c2 = (unsigned char)s[i + 1];

                // Ё: D0 81 -> D1 91
                if (c2 == 0x81) {
                    s[i]     = (char)0xD1;
                    s[i + 1] = (char)0x91;
                    i += 2;
                    continue;
                }

                // А..П: D0 90..9F -> D0 B0..BF
                if (c2 >= 0x90 && c2 <= 0x9F) {
                    s[i + 1] = (char)(c2 + 0x20);
                    i += 2;
                    continue;
                }

                // Р..Я: D0 A0..AF -> D1 80..8F
                if (c2 >= 0xA0 && c2 <= 0xAF) {
                    s[i]     = (char)0xD1;
                    s[i + 1] = (char)(c2 - 0x20);
                    i += 2;
                    continue;
                }
            }

            // пропускаем любой другой UTF-8 символ
            size_t step = (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
            i += step;
        }
        return s;
    };

    // удалить 1 UTF-8 символ с конца
    auto pop_utf8 = [](std::string& s) {
        if (s.empty()) return;
        size_t i = s.size() - 1;
        while (i > 0 && (((unsigned char)s[i] & 0xC0) == 0x80)) --i;
        s.erase(i);
    };

    // длина в "буквах" UTF-8
    auto utf8_len = [](const std::string& s) {
        size_t n = 0;
        for (size_t i = 0; i < s.size(); ) {
            unsigned char c = (unsigned char)s[i];
            size_t step = (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0) ? 2 : ((c & 0xF0) == 0xE0) ? 3 : 4;
            i += step;
            ++n;
        }
        return n;
    };

    auto ends_with = [](const std::string& s, const std::string& suf) {
        return s.size() >= suf.size() &&
               s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
    };

    // Конечный список окончаний
    const std::vector<std::pair<std::string,int>> suffixes = {
        // глагольные/деепричастные
        { "ся",  2 }, { "сь", 2 },   // продаются -> продают
        { "ует", 3 },                // трамбует -> трамб
        { "уя",  2 },                // трамбуя -> трамб
        { "ает", 3 },                // разрывает -> разрыв
        { "яет", 3 },                // удобряет -> удобр
        { "ют",  2 },                // дренькают -> дренька
        { "ив",  2 },                // продырявив -> продыряв

        // прилагательные/падежи 
        { "ями", 3 }, { "ами", 3 }, { "ыми", 3 }, { "ими", 3 },
        { "ым",  2 }, { "им",  2 }, // добрым -> добр
        { "ах",  2 }, { "ях",  2 }, { "ам",  2 }, { "ям",  2 }, { "ом",  2 }, { "ем",  2 },
        { "ов",  2 }, { "ев",  2 },
        { "ой",  2 }, { "ей",  2 }, { "ый",  2 }, { "ий",  2 },
        { "ая",  2 }, { "яя",  2 }, { "ые",  2 }, { "ие",  2 },
        { "ых",  2 }, { "их",  2 }, { "ую",  2 }, { "юю",  2 },

        // односимвольные
        { "а", 1 }, { "я", 1 }, { "ы", 1 }, { "и", 1 }, { "о", 1 }, { "е", 1 }, { "у", 1 }, { "ю", 1 },
        { "ь", 1 }, { "й", 1 }
    };

    auto stem = [&](std::string s) {
        size_t L = utf8_len(s);
        if (L <= 3) return s;  // не режем короткие: "эти", "и", "в", "к"

        // спец-случай: ядер -> ядр, вёдер -> вёдр
        if (ends_with(s, "ер") && L > 3) {
            pop_utf8(s); // р
            pop_utf8(s); // е
            s += "р";
            return s;
        }

        for (const auto& p : suffixes) {
            const std::string& suf = p.first;
            int k = p.second;

            if (ends_with(s, suf) && L > (size_t)k + 1) {
                for (int j = 0; j < k; ++j) pop_utf8(s);
                return s;
            }
        }
        return s;
    };

    std::vector<std::string> words;
    for (std::string w; in >> w; ) {
        w = clean(w);
        w = to_lower_ru_utf8(w);
        if (w.empty() || w == "-") continue;
        words.push_back(w);
    }

    for (size_t i = 0; i < words.size(); ++i)
        std::cout << (i + 1) << " - " << words[i] << "\n";

    std::vector<std::string> uniq;
    std::vector<int> counts;

    std::for_each(words.begin(), words.end(), [&](const std::string& w) {
        std::string key = stem(w);

        auto it = std::find(uniq.begin(), uniq.end(), key); // find
        if (it == uniq.end()) {
            uniq.push_back(key);
            counts.push_back(1);
        } else {
            counts[std::distance(uniq.begin(), it)]++;
        }
    });

    std::cout << "\n=== Повторы (по основе) ===\n";
    for (size_t i = 0; i < uniq.size(); ++i)
        std::cout << uniq[i] << " : " << counts[i] << "\n";

    return 0;
}
