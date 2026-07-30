#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> result;
    std::stringstream stream(line);
    for (std::string value; std::getline(stream, value, ',');) result.push_back(value);
    return result;
}

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    std::ifstream left(argv[1]);
    std::ifstream right(argv[2]);
    if (!left || !right) return 3;
    std::string left_line, right_line;
    double maximum{};
    std::size_t values{};
    while (std::getline(left, left_line)) {
        if (!std::getline(right, right_line)) return 4;
        const auto left_values = split(left_line);
        const auto right_values = split(right_line);
        if (left_values.size() != right_values.size()) return 5;
        for (std::size_t index = 0; index < left_values.size(); ++index) {
            if (!left_values[index].empty() && left_values[index].front() == '"') {
                if (left_values[index] != right_values[index]) return 6;
                continue;
            }
            const double first = std::stod(left_values[index]);
            const double second = std::stod(right_values[index]);
            maximum = std::max(maximum,
                std::abs(first - second) / std::max(1.0, std::abs(first)));
            ++values;
        }
    }
    if (std::getline(right, right_line)) return 7;
    std::cout.precision(17);
    std::cout << maximum << ' ' << values << '\n';
    return maximum <= 1.0e-8 ? 0 : 8;
}
