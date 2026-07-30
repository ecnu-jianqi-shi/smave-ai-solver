#pragma once

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace smave {

class Expression {
public:
    struct Node;

    explicit Expression(std::string source);
    Expression(const Expression& other);
    Expression& operator=(const Expression& other);
    Expression(Expression&&) noexcept;
    Expression& operator=(Expression&&) noexcept;
    ~Expression();

    [[nodiscard]] double evaluate(
        const std::unordered_map<std::string, double>& values) const;
    [[nodiscard]] std::optional<double> directional_derivative(
        const std::unordered_map<std::string, double>& values,
        const std::unordered_map<std::string, double>& directions) const;
    [[nodiscard]] const std::set<std::string>& names() const noexcept;
    [[nodiscard]] const std::string& source() const noexcept;
    [[nodiscard]] std::optional<std::vector<double>> constant_linear_coefficients(
        const std::vector<std::string>& unknowns) const;

private:
    struct Parser;
    std::string source_;
    std::unique_ptr<Node> root_;
    std::set<std::string> names_;
};

}  // namespace smave
