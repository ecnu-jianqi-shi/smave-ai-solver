#include "smave/expression.hpp"

#include <cctype>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace smave {

struct Expression::Node {
    enum class Kind { number, variable, unary, binary, function };
    Kind kind{Kind::number};
    double number{};
    std::string text;
    char operation{};
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
    std::vector<std::unique_ptr<Node>> arguments;
};

struct Expression::Parser {
    explicit Parser(std::string_view source, std::set<std::string>& names)
        : source(source), names(names) {}

    std::unique_ptr<Node> parse() {
        auto result = expression();
        whitespace();
        if (position != source.size()) {
            fail("unexpected token");
        }
        return result;
    }

    std::unique_ptr<Node> expression() {
        auto node = term();
        while (true) {
            whitespace();
            if (!consume('+') && !consume('-')) {
                return node;
            }
            const char operation = source[position - 1];
            auto parent = std::make_unique<Node>();
            parent->kind = Node::Kind::binary;
            parent->operation = operation;
            parent->left = std::move(node);
            parent->right = term();
            node = std::move(parent);
        }
    }

    std::unique_ptr<Node> term() {
        auto node = power();
        while (true) {
            whitespace();
            if (!consume('*') && !consume('/')) {
                return node;
            }
            const char operation = source[position - 1];
            auto parent = std::make_unique<Node>();
            parent->kind = Node::Kind::binary;
            parent->operation = operation;
            parent->left = std::move(node);
            parent->right = power();
            node = std::move(parent);
        }
    }

    std::unique_ptr<Node> power() {
        auto node = unary();
        whitespace();
        if (consume('^')) {
            auto parent = std::make_unique<Node>();
            parent->kind = Node::Kind::binary;
            parent->operation = '^';
            parent->left = std::move(node);
            parent->right = power();
            return parent;
        }
        return node;
    }

    std::unique_ptr<Node> unary() {
        whitespace();
        if (consume('+') || consume('-')) {
            auto node = std::make_unique<Node>();
            node->kind = Node::Kind::unary;
            node->operation = source[position - 1];
            node->left = unary();
            return node;
        }
        return primary();
    }

    std::unique_ptr<Node> primary() {
        whitespace();
        if (consume('(')) {
            auto node = expression();
            whitespace();
            if (!consume(')')) {
                fail("missing closing parenthesis");
            }
            return node;
        }
        if (position < source.size() &&
            (std::isdigit(static_cast<unsigned char>(source[position])) ||
             source[position] == '.')) {
            return number();
        }
        if (position < source.size() &&
            (std::isalpha(static_cast<unsigned char>(source[position])) ||
             source[position] == '_')) {
            return identifier();
        }
        fail("expected number, variable, or parenthesis");
    }

    std::unique_ptr<Node> number() {
        const std::size_t start = position;
        while (position < source.size() &&
               (std::isdigit(static_cast<unsigned char>(source[position])) ||
                source[position] == '.' || source[position] == 'e' ||
                source[position] == 'E' || source[position] == '+' ||
                source[position] == '-')) {
            if ((source[position] == '+' || source[position] == '-') &&
                position > start && source[position - 1] != 'e' &&
                source[position - 1] != 'E') {
                break;
            }
            ++position;
        }
        auto node = std::make_unique<Node>();
        node->kind = Node::Kind::number;
        try {
            node->number = std::stod(std::string(source.substr(start, position - start)));
        } catch (const std::exception&) {
            fail("invalid number");
        }
        return node;
    }

    std::unique_ptr<Node> identifier() {
        const std::size_t start = position++;
        while (position < source.size() &&
               (std::isalnum(static_cast<unsigned char>(source[position])) ||
                source[position] == '_')) {
            ++position;
        }
        const std::string name(source.substr(start, position - start));
        whitespace();
        if (consume('(')) {
            static const std::set<std::string> allowed{
                "abs", "cos", "exp", "log", "sin", "sqrt", "tan"};
            if (!allowed.contains(name)) {
                fail("unsupported function: " + name);
            }
            auto node = std::make_unique<Node>();
            node->kind = Node::Kind::function;
            node->text = name;
            node->arguments.push_back(expression());
            whitespace();
            if (!consume(')')) {
                fail("missing function closing parenthesis");
            }
            return node;
        }
        auto node = std::make_unique<Node>();
        node->kind = Node::Kind::variable;
        node->text = name;
        if (name != "pi" && name != "e") {
            names.insert(name);
        }
        return node;
    }

    bool consume(char expected) {
        if (position < source.size() && source[position] == expected) {
            ++position;
            return true;
        }
        return false;
    }

    void whitespace() {
        while (position < source.size() &&
               std::isspace(static_cast<unsigned char>(source[position]))) {
            ++position;
        }
    }

    [[noreturn]] void fail(const std::string& message) const {
        throw std::invalid_argument(
            "expression error at " + std::to_string(position) + ": " + message);
    }

    std::string_view source;
    std::size_t position{};
    std::set<std::string>& names;
};

namespace {

double evaluate_node(
    const Expression::Node& node,
    const std::unordered_map<std::string, double>& values) {
    using Kind = Expression::Node::Kind;
    if (node.kind == Kind::number) {
        return node.number;
    }
    if (node.kind == Kind::variable) {
        if (node.text == "pi") {
            return std::acos(-1.0);
        }
        if (node.text == "e") {
            return std::exp(1.0);
        }
        const auto iterator = values.find(node.text);
        if (iterator == values.end()) {
            throw std::invalid_argument("missing value for " + node.text);
        }
        return iterator->second;
    }
    if (node.kind == Kind::unary) {
        const double value = evaluate_node(*node.left, values);
        return node.operation == '-' ? -value : value;
    }
    if (node.kind == Kind::binary) {
        const double left = evaluate_node(*node.left, values);
        const double right = evaluate_node(*node.right, values);
        switch (node.operation) {
            case '+': return left + right;
            case '-': return left - right;
            case '*': return left * right;
            case '/': return left / right;
            case '^': return std::pow(left, right);
            default: throw std::logic_error("unknown binary operation");
        }
    }
    const double argument = evaluate_node(*node.arguments.front(), values);
    if (node.text == "abs") return std::abs(argument);
    if (node.text == "cos") return std::cos(argument);
    if (node.text == "exp") return std::exp(argument);
    if (node.text == "log") return std::log(argument);
    if (node.text == "sin") return std::sin(argument);
    if (node.text == "sqrt") return std::sqrt(argument);
    if (node.text == "tan") return std::tan(argument);
    throw std::logic_error("unknown function");
}

struct DualValue {
    double value{};
    double derivative{};
};

std::optional<DualValue> evaluate_dual(
    const Expression::Node& node,
    const std::unordered_map<std::string, double>& values,
    const std::unordered_map<std::string, double>& directions) {
    using Kind = Expression::Node::Kind;
    DualValue result;
    if (node.kind == Kind::number) {
        result.value = node.number;
    } else if (node.kind == Kind::variable) {
        if (node.text == "pi") {
            result.value = std::acos(-1.0);
        } else if (node.text == "e") {
            result.value = std::exp(1.0);
        } else {
            const auto value = values.find(node.text);
            if (value == values.end()) {
                throw std::invalid_argument("missing value for " + node.text);
            }
            result.value = value->second;
            const auto direction = directions.find(node.text);
            if (direction != directions.end()) result.derivative = direction->second;
        }
    } else if (node.kind == Kind::unary) {
        const auto argument = evaluate_dual(*node.left, values, directions);
        if (!argument.has_value()) return std::nullopt;
        result = *argument;
        if (node.operation == '-') {
            result.value = -result.value;
            result.derivative = -result.derivative;
        }
    } else if (node.kind == Kind::binary) {
        const auto left = evaluate_dual(*node.left, values, directions);
        const auto right = evaluate_dual(*node.right, values, directions);
        if (!left.has_value() || !right.has_value()) return std::nullopt;
        switch (node.operation) {
            case '+':
                result = {left->value + right->value,
                          left->derivative + right->derivative};
                break;
            case '-':
                result = {left->value - right->value,
                          left->derivative - right->derivative};
                break;
            case '*':
                result = {left->value * right->value,
                          left->derivative * right->value +
                              left->value * right->derivative};
                break;
            case '/':
                if (right->value == 0.0) return std::nullopt;
                result = {left->value / right->value,
                          (left->derivative * right->value -
                           left->value * right->derivative) /
                              (right->value * right->value)};
                break;
            case '^': {
                result.value = std::pow(left->value, right->value);
                if (right->derivative == 0.0) {
                    if (right->value == 0.0) {
                        result.derivative = 0.0;
                    } else {
                        result.derivative = right->value *
                            std::pow(left->value, right->value - 1.0) *
                            left->derivative;
                    }
                } else {
                    if (!(left->value > 0.0)) return std::nullopt;
                    result.derivative = result.value *
                        (right->derivative * std::log(left->value) +
                         right->value * left->derivative / left->value);
                }
                break;
            }
            default: throw std::logic_error("unknown binary operation");
        }
    } else {
        const auto argument = evaluate_dual(
            *node.arguments.front(), values, directions);
        if (!argument.has_value()) return std::nullopt;
        if (node.text == "abs") {
            if (argument->value == 0.0 && argument->derivative != 0.0) {
                return std::nullopt;
            }
            result = {std::abs(argument->value),
                      argument->value < 0.0
                          ? -argument->derivative
                          : argument->derivative};
        } else if (node.text == "cos") {
            result = {std::cos(argument->value),
                      -std::sin(argument->value) * argument->derivative};
        } else if (node.text == "exp") {
            result.value = std::exp(argument->value);
            result.derivative = result.value * argument->derivative;
        } else if (node.text == "log") {
            if (!(argument->value > 0.0)) return std::nullopt;
            result = {std::log(argument->value),
                      argument->derivative / argument->value};
        } else if (node.text == "sin") {
            result = {std::sin(argument->value),
                      std::cos(argument->value) * argument->derivative};
        } else if (node.text == "sqrt") {
            if (argument->value < 0.0 ||
                (argument->value == 0.0 && argument->derivative != 0.0)) {
                return std::nullopt;
            }
            result.value = std::sqrt(argument->value);
            result.derivative = result.value == 0.0
                ? 0.0
                : argument->derivative / (2.0 * result.value);
        } else if (node.text == "tan") {
            const double cosine = std::cos(argument->value);
            if (cosine == 0.0) return std::nullopt;
            result = {std::tan(argument->value),
                      argument->derivative / (cosine * cosine)};
        } else {
            throw std::logic_error("unknown function");
        }
    }
    if (!std::isfinite(result.value) || !std::isfinite(result.derivative)) {
        return std::nullopt;
    }
    return result;
}

struct LinearForm {
    bool valid{true};
    bool unknown_free{true};
    bool constant{true};
    double constant_value{};
    std::vector<double> coefficients;
};

LinearForm invalid_form(std::size_t size) {
    LinearForm result;
    result.valid = false;
    result.coefficients.resize(size);
    return result;
}

LinearForm linear_form(
    const Expression::Node& node,
    const std::unordered_map<std::string, std::size_t>& unknown_positions,
    std::size_t size) {
    using Kind = Expression::Node::Kind;
    LinearForm result;
    result.coefficients.resize(size);
    if (node.kind == Kind::number) {
        result.constant_value = node.number;
        return result;
    }
    if (node.kind == Kind::variable) {
        const auto unknown = unknown_positions.find(node.text);
        if (unknown != unknown_positions.end()) {
            result.unknown_free = false;
            result.constant = false;
            result.constant_value = 0.0;
            result.coefficients[unknown->second] = 1.0;
            return result;
        }
        if (node.text == "pi") {
            result.constant_value = std::acos(-1.0);
        } else if (node.text == "e") {
            result.constant_value = std::exp(1.0);
        } else {
            result.constant = false;
        }
        return result;
    }
    if (node.kind == Kind::unary) {
        result = linear_form(*node.left, unknown_positions, size);
        if (node.operation == '-') {
            result.constant_value = -result.constant_value;
            for (double& coefficient : result.coefficients) coefficient = -coefficient;
        }
        return result;
    }
    if (node.kind == Kind::function) {
        const auto argument = linear_form(
            *node.arguments.front(), unknown_positions, size);
        if (!argument.valid || !argument.unknown_free) return invalid_form(size);
        result.constant = argument.constant;
        if (argument.constant) {
            result.constant_value = evaluate_node(node, {});
        }
        return result;
    }
    auto left = linear_form(*node.left, unknown_positions, size);
    auto right = linear_form(*node.right, unknown_positions, size);
    if (!left.valid || !right.valid) return invalid_form(size);
    if (node.operation == '+' || node.operation == '-') {
        const double sign = node.operation == '+' ? 1.0 : -1.0;
        result.unknown_free = left.unknown_free && right.unknown_free;
        result.constant = left.constant && right.constant;
        result.constant_value = left.constant_value + sign * right.constant_value;
        for (std::size_t index = 0; index < size; ++index) {
            result.coefficients[index] =
                left.coefficients[index] + sign * right.coefficients[index];
        }
        return result;
    }
    if (node.operation == '*') {
        if (left.unknown_free && right.unknown_free) {
            result.constant = left.constant && right.constant;
            if (result.constant) {
                result.constant_value = left.constant_value * right.constant_value;
            }
            return result;
        }
        const LinearForm* affine = nullptr;
        const LinearForm* scale = nullptr;
        if (!left.unknown_free && right.constant) {
            affine = &left; scale = &right;
        } else if (!right.unknown_free && left.constant) {
            affine = &right; scale = &left;
        } else {
            return invalid_form(size);
        }
        result.unknown_free = false;
        result.constant = affine->constant;
        result.constant_value = affine->constant_value * scale->constant_value;
        for (std::size_t index = 0; index < size; ++index) {
            result.coefficients[index] =
                affine->coefficients[index] * scale->constant_value;
        }
        return result;
    }
    if (node.operation == '/') {
        if (!right.unknown_free || !right.constant || right.constant_value == 0.0) {
            return invalid_form(size);
        }
        result = left;
        result.constant_value /= right.constant_value;
        for (double& coefficient : result.coefficients) {
            coefficient /= right.constant_value;
        }
        return result;
    }
    if (node.operation == '^') {
        if (!left.unknown_free || !right.unknown_free) return invalid_form(size);
        result.constant = left.constant && right.constant;
        if (result.constant) {
            result.constant_value = std::pow(left.constant_value, right.constant_value);
        }
        return result;
    }
    return invalid_form(size);
}

}  // namespace

Expression::Expression(std::string source) : source_(std::move(source)) {
    Parser parser(source_, names_);
    root_ = parser.parse();
}

Expression::Expression(const Expression& other) : Expression(other.source_) {}

Expression& Expression::operator=(const Expression& other) {
    if (this != &other) {
        Expression copy(other);
        *this = std::move(copy);
    }
    return *this;
}

Expression::Expression(Expression&&) noexcept = default;
Expression& Expression::operator=(Expression&&) noexcept = default;
Expression::~Expression() = default;

double Expression::evaluate(
    const std::unordered_map<std::string, double>& values) const {
    return evaluate_node(*root_, values);
}

std::optional<double> Expression::directional_derivative(
    const std::unordered_map<std::string, double>& values,
    const std::unordered_map<std::string, double>& directions) const {
    const auto result = evaluate_dual(*root_, values, directions);
    if (!result.has_value()) return std::nullopt;
    return result->derivative;
}

const std::set<std::string>& Expression::names() const noexcept { return names_; }
const std::string& Expression::source() const noexcept { return source_; }

std::optional<std::vector<double>> Expression::constant_linear_coefficients(
    const std::vector<std::string>& unknowns) const {
    std::unordered_map<std::string, std::size_t> positions;
    for (std::size_t index = 0; index < unknowns.size(); ++index) {
        if (!positions.emplace(unknowns[index], index).second) return std::nullopt;
    }
    auto form = linear_form(*root_, positions, unknowns.size());
    if (!form.valid) return std::nullopt;
    return form.coefficients;
}

}  // namespace smave
