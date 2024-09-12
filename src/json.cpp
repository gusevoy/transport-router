#include "json.h"

using namespace std;

namespace json {

    namespace {

        using namespace std::literals;

        // Контекст вывода, хранит ссылку на поток вывода и текущий отсуп
        struct PrintContext {
            std::ostream& out;
            int indent_step = 4;
            int indent = 0;

            void PrintIndent() const {
                for (int i = 0; i < indent; ++i) {
                    out.put(' ');
                }
            }

            // Возвращает новый контекст вывода с увеличенным смещением
            PrintContext Indented() const {
                return { out, indent_step, indent_step + indent };
            }
        };

        void PrintNode(const Node& node, PrintContext& ctx);

        // double, int
        template <typename ValueT>
        void PrintValue(const ValueT& value, PrintContext& ctx) {
            ctx.out << value;
        }

        // string
        void PrintValue(std::string str, PrintContext& ctx) {
            ctx.out.put('\"');
            for (char c : str) {
                switch (c) {
                case '\n':
                    ctx.out << "\\n"s;
                    break;
                case '\r':
                    ctx.out << "\\r"s;
                    break;
                case '\"':
                    ctx.out << "\\\""s;
                    break;
                case '\t':
                    ctx.out << "\\t"s;
                    break;
                case '\\':
                    ctx.out << "\\\\"s;
                    break;
                default:
                    ctx.out.put(c);
                }
            }
            ctx.out.put('\"');
        }

        // bool
        void PrintValue(bool value, PrintContext& ctx) {
            ctx.out << (value ? "true"s : "false"s);
        }

        // Array
        void PrintValue(const Array& value, PrintContext& ctx) {
            PrintContext indented = ctx.Indented();
            ctx.out << "["s;
            bool is_first = true;
            for (const Node& node : value) {
                if (is_first) {
                    is_first = false;
                }
                else {
                    indented.out.put(',');
                }
                indented.out.put('\n');
                indented.PrintIndent();
                PrintNode(node, indented);
            }
            ctx.out << "\n"s;
            ctx.PrintIndent();
            ctx.out << "]"s;
        }

        // Dict
        void PrintValue(const Dict& value, PrintContext& ctx) {
            PrintContext indented = ctx.Indented();
            ctx.out << "{"s;
            bool is_first = true;
            //for (const pair<std::string, Node>& kvp : value) {
            for (auto const& kvp : value) {
                if (is_first) {
                    is_first = false;
                }
                else {
                    indented.out.put(',');
                }
                indented.out.put('\n');
                indented.PrintIndent();
                indented.out << "\""s << kvp.first << "\": "s;
                PrintNode(kvp.second, indented);
            }
            ctx.out << "\n"s;
            ctx.PrintIndent();
            ctx.out << "}"s;
        }

        // nullptr_t
        void PrintValue(std::nullptr_t, PrintContext& ctx) {
            ctx.out << "null"sv;
        }

        void PrintNode(const Node& node, PrintContext& ctx) {
            std::visit([&ctx](const auto& value) { PrintValue(value, ctx); }, node.GetValue());
        }

        Node LoadNode(std::istream& input);
        Node LoadString(std::istream& input);

        std::string LoadLiteral(std::istream& input) {
            std::string s;
            while (std::isalpha(input.peek())) {
                s.push_back(static_cast<char>(input.get()));
            }
            return s;
        }

        Node LoadArray(std::istream& input) {
            std::vector<Node> result;

            for (char c; input >> c && c != ']';) {
                if (c != ',') {
                    input.putback(c);
                }
                result.push_back(LoadNode(input));
            }
            if (!input) {
                throw ParsingError("Array parsing error"s);
            }
            return Node{ std::move(result) };
        }

        Node LoadDict(std::istream& input) {
            Dict dict;

            for (char c; input >> c && c != '}';) {
                if (c == '"') {
                    std::string key = LoadString(input).AsString();
                    if (input >> c && c == ':') {
                        if (dict.find(key) != dict.end()) {
                            throw ParsingError("Duplicate key '"s + key + "' have been found");
                        }
                        dict.emplace(std::move(key), LoadNode(input));
                    }
                    else {
                        throw ParsingError(": is expected but '"s + c + "' has been found"s);
                    }
                }
                else if (c != ',') {
                    throw ParsingError(R"(',' is expected but ')"s + c + "' has been found"s);
                }
            }
            if (!input) {
                throw ParsingError("Dictionary parsing error"s);
            }
            return Node(std::move(dict));
        }

        Node LoadString(std::istream& input) {
            auto it = std::istreambuf_iterator<char>(input);
            auto end = std::istreambuf_iterator<char>();
            std::string s;
            while (true) {
                if (it == end) {
                    throw ParsingError("String parsing error");
                }
                const char ch = *it;
                if (ch == '"') {
                    ++it;
                    break;
                }
                else if (ch == '\\') {
                    ++it;
                    if (it == end) {
                        throw ParsingError("String parsing error");
                    }
                    const char escaped_char = *(it);
                    switch (escaped_char) {
                    case 'n':
                        s.push_back('\n');
                        break;
                    case 't':
                        s.push_back('\t');
                        break;
                    case 'r':
                        s.push_back('\r');
                        break;
                    case '"':
                        s.push_back('"');
                        break;
                    case '\\':
                        s.push_back('\\');
                        break;
                    default:
                        throw ParsingError("Unrecognized escape sequence \\"s + escaped_char);
                    }
                }
                else if (ch == '\n' || ch == '\r') {
                    throw ParsingError("Unexpected end of line"s);
                }
                else {
                    s.push_back(ch);
                }
                ++it;
            }

            return Node(std::move(s));
        }

        Node LoadBool(std::istream& input) {
            const auto s = LoadLiteral(input);
            if (s == "true"sv) {
                return Node{ true };
            }
            else if (s == "false"sv) {
                return Node{ false };
            }
            else {
                throw ParsingError("Failed to parse '"s + s + "' as bool"s);
            }
        }

        Node LoadNull(std::istream& input) {
            if (auto literal = LoadLiteral(input); literal == "null"sv) {
                return Node{ nullptr };
            }
            else {
                throw ParsingError("Failed to parse '"s + literal + "' as null"s);
            }
        }

        Node LoadNumber(std::istream& input) {
            std::string parsed_num;

            // Считывает в parsed_num очередной символ из input
            auto read_char = [&parsed_num, &input] {
                parsed_num += static_cast<char>(input.get());
                if (!input) {
                    throw ParsingError("Failed to read number from stream"s);
                }
                };

            // Считывает одну или более цифр в parsed_num из input
            auto read_digits = [&input, read_char] {
                if (!std::isdigit(input.peek())) {
                    throw ParsingError("A digit is expected"s);
                }
                while (std::isdigit(input.peek())) {
                    read_char();
                }
                };

            if (input.peek() == '-') {
                read_char();
            }
            // Парсим целую часть числа
            if (input.peek() == '0') {
                read_char();
                // После 0 в JSON не могут идти другие цифры
            }
            else {
                read_digits();
            }

            bool is_int = true;
            // Парсим дробную часть числа
            if (input.peek() == '.') {
                read_char();
                read_digits();
                is_int = false;
            }

            // Парсим экспоненциальную часть числа
            if (int ch = input.peek(); ch == 'e' || ch == 'E') {
                read_char();
                if (ch = input.peek(); ch == '+' || ch == '-') {
                    read_char();
                }
                read_digits();
                is_int = false;
            }

            try {
                if (is_int) {
                    // Сначала пробуем преобразовать строку в int
                    try {
                        return std::stoi(parsed_num);
                    }
                    catch (...) {
                        // В случае неудачи, например, при переполнении
                        // код ниже попробует преобразовать строку в double
                    }
                }
                return std::stod(parsed_num);
            }
            catch (...) {
                throw ParsingError("Failed to convert "s + parsed_num + " to number"s);
            }
        }

        Node LoadNode(std::istream& input) {
            char c;
            if (!(input >> c)) {
                throw ParsingError("Unexpected EOF"s);
            }
            switch (c) {
            case '[':
                return LoadArray(input);
            case '{':
                return LoadDict(input);
            case '"':
                return LoadString(input);
            case 't':
                // Атрибут [[fallthrough]] (провалиться) ничего не делает, и является
                // подсказкой компилятору и человеку, что здесь программист явно задумывал
                // разрешить переход к инструкции следующей ветки case, а не случайно забыл
                // написать break, return или throw.
                // В данном случае, встретив t или f, переходим к попытке парсинга
                // литералов true либо false
                [[fallthrough]];
            case 'f':
                input.putback(c);
                return LoadBool(input);
            case 'n':
                input.putback(c);
                return LoadNull(input);
            default:
                input.putback(c);
                return LoadNumber(input);
            }
        }

    }  // namespace

    const Node::Value& Node::GetValue() const{
        return *this;
    }

    Node::Value& Node::GetValue() {
        return *this;
    }

    bool Node::IsInt() const {
        return std::holds_alternative<int>(*this);
    }
    bool Node::IsDouble() const {
        return IsInt() || std::holds_alternative<double>(*this);
    }
    bool Node::IsPureDouble() const {
        return std::holds_alternative<double>(*this);
    }
    bool Node::IsBool() const {
        return std::holds_alternative<bool>(*this);
    }
    bool Node::IsString() const {
        return std::holds_alternative<std::string>(*this);
    }
    bool Node::IsNull() const {
        return std::holds_alternative<std::nullptr_t>(*this);
    }
    bool Node::IsArray() const {
        return std::holds_alternative<Array>(*this);
    }
    bool Node::IsDict() const {
        return std::holds_alternative<Dict>(*this);
    }

    int Node::AsInt() const {
        return AsType<int>();
    }
    bool Node::AsBool() const {
        return AsType<bool>();
    }
    double Node::AsDouble() const {
        if (std::holds_alternative<int>(*this)) {
            return static_cast<double>(AsType<int>());
        }
        return AsType<double>();
    }
    const std::string& Node::AsString() const {
        return AsType<std::string>();
    }
    const Array& Node::AsArray() const {
        return AsType<Array>();
    }
    const Dict& Node::AsMap() const {
        return AsType<Dict>();
    }
    
    bool operator==(const Node& lhs, const Node& rhs) {
        return lhs.GetValue() == rhs.GetValue();
    }

    bool operator!=(const Node& lhs, const Node& rhs) {
        return lhs.GetValue() != rhs.GetValue();
    }
    
    Document::Document(Node root)
        : root_(move(root)) {
    }

    const Node& Document::GetRoot() const {
        return root_;
    }

    Document Load(istream& input) {
        return Document{ LoadNode(input) };
    }

    bool operator==(const Document& lhs, const Document& rhs) {
        return lhs.GetRoot() == rhs.GetRoot();
    }

    bool operator!=(const Document& lhs, const Document& rhs) {
        return lhs.GetRoot() != rhs.GetRoot();
    }

    void Print(const Document& doc, std::ostream& output) {
        PrintContext ctx{ output, 4, 0 };
        PrintNode(doc.GetRoot(), ctx);
    }

}  // namespace json