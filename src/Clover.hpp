#pragma once
#include <cstdint>
#include <string>

#if (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || (__cplusplus >= 201703L)
#include <string_view>
namespace Clover { using string_view = std::string_view; }
#else
namespace Clover {
    class string_view {
    private:
        const char* m_ptr = nullptr;
        size_t m_size = 0;

    public:
        constexpr string_view() noexcept = default;

        string_view(const char* str) noexcept
            : m_ptr(str), m_size(str ? traits_length(str) : 0) {
        }

        constexpr string_view(const char* ptr, size_t size) noexcept
            : m_ptr(ptr), m_size(size) {
        }

        string_view(const std::string& str) noexcept
            : m_ptr(str.data()), m_size(str.size()) {
        }

        constexpr const char* data() const noexcept { return m_ptr; }
        constexpr size_t size() const noexcept { return m_size; }
        constexpr bool empty() const noexcept { return m_size == 0; }

        constexpr char operator[](size_t index) const noexcept { return m_ptr[index]; }
        constexpr const char* begin() const noexcept { return m_ptr; }
        constexpr const char* end() const noexcept { return m_ptr + m_size; }

        string_view substr(size_t pos, size_t count = -1) const noexcept {
            if (pos >= m_size) return string_view();
            size_t rcount = (count == static_cast<size_t>(-1) || pos + count > m_size)
                ? m_size - pos
                : count;
            return string_view(m_ptr + pos, rcount);
        }

        size_t find(char c, size_t pos = 0) const noexcept {
            for (size_t i = pos; i < m_size; ++i) {
                if (m_ptr[i] == c) return i;
            }
            return -1;
        }

        bool operator==(const char* rhs) const noexcept {
            size_t i = 0;
            while (i < m_size && rhs[i] != '\0') {
                if (m_ptr[i] != rhs[i]) return false;
                ++i;
            }
            return i == m_size && rhs[i] == '\0';
        }

        static constexpr size_t npos = static_cast<size_t>(-1);

    private:
        static size_t traits_length(const char* s) noexcept {
            size_t len = 0;
            while (s[len] != '\0') ++len;
            return len;
        }
    };
}
#endif


namespace Clover {
    struct Version {
        uint16_t major = 0;
        uint16_t minor = 0;
        uint16_t patch = 0;

        //

        Version() = default;

        Version(const string_view str) noexcept {
            parse(str);
        }

        Version(uint16_t major, uint16_t minor, uint16_t patch) noexcept
			: major(major), minor(minor), patch(patch) {
		}

        void parse(const string_view str) noexcept {
            uint32_t parts[3] = { 0, 0, 0 };
            constexpr size_t maxParts = 3;

            uint8_t idx = 0;

            for (char c : str) {
                if (c == '.') {
                    if (idx == maxParts - 1) {
                        break;
                    }
                    idx++;
                }
                else {
                    parts[idx] = parts[idx] * 10 + (c - '0');
                }
            }
            major = static_cast<uint16_t>(parts[0]);
            minor = static_cast<uint16_t>(parts[1]);
            patch = static_cast<uint16_t>(parts[2]);
        }

        //

        friend bool operator==(const Version  lhs, const Version  rhs) noexcept {
            return (lhs.major == rhs.major) && (lhs.minor == rhs.minor) && (lhs.patch == rhs.patch);
        }

        friend bool operator<(const Version  lhs, const Version  rhs) noexcept {
            return (lhs.major < rhs.major) |
                ((lhs.major == rhs.major) & (lhs.minor < rhs.minor)) |
                ((lhs.major == rhs.major) & (lhs.minor == rhs.minor) & (lhs.patch < rhs.patch));
        }

        friend bool operator!=(const Version  lhs, const Version  rhs) noexcept { return !(lhs == rhs); }

        friend bool operator>(const Version  lhs, const Version  rhs) noexcept { return rhs < lhs; }

        friend bool operator<=(const Version  lhs, const Version  rhs) noexcept { return !(rhs < lhs); }

        friend bool operator>=(const Version  lhs, const Version  rhs) noexcept { return !(lhs < rhs); }

        bool Tilde(const Version other) const noexcept {
            return (major == other.major) && (minor == other.minor) && (patch >= other.patch);
        }

        bool Caret(const Version req) const noexcept {
            if (major != req.major) {
                return false;
            }

            if (major > 0) {
                return (minor > req.minor) ||
                    (minor == req.minor && patch >= req.patch);
            }

            if (minor > 0) {
                return (minor == req.minor) && (patch >= req.patch);
            }

            return (minor == req.minor) && (patch == req.patch);
        }
    };

    enum class VersionOp : uint8_t {
        Exact,      // "1.2.0" or "=1.2.0"
        Greater,    // ">1.2.0"
        GreaterEq,  // ">=1.2.0"
        Less,       // "<1.2.0"
        LessEq,     // "<=1.2.0"
        Caret,      // "^1.2.0" (Compatible minor)
        Tilde,      // "~1.2.0" (Compatible patch)
        Any         // "*"
    };

    enum class PreReleaseType : uint8_t {
        None = 0, Alpha = 1, Beta = 2, RC = 3
    };

    struct VersionConstraint {
        VersionOp op = VersionOp::Exact;
        // there is single byte of padding byte here to align the next member
        Version target{};

        //

        VersionConstraint() = default;
        VersionConstraint(const string_view str) {
            parse(str);
        }

        VersionConstraint(VersionOp op, Version ver) noexcept : op(op), target(ver) {};

        VersionConstraint(VersionOp op, uint16_t major, uint16_t minor, uint16_t patch) noexcept : op(op), target(major, minor, patch) {}

        void parse(const string_view str) noexcept {
            if (str.empty()) return;

            if (str == "*") {
                op = VersionOp::Any;
                return;
            }

            size_t pos = 0;
            char firstChar = str[0];
            const size_t size = str.size();

            if (size == 1 && firstChar == '*') {
                op = VersionOp::Any;
                return;
            }

            switch (firstChar) {
            case '>':
                if (size > 1 && str[1] == '=') { op = VersionOp::GreaterEq; pos = 2; }
                else { op = VersionOp::Greater; pos = 1; }
                break;
            case '<':
                if (size > 1 && str[1] == '=') { op = VersionOp::LessEq; pos = 2; }
                else { op = VersionOp::Less; pos = 1; }
                break;
            case '=': op = VersionOp::Exact; pos = 1; break;
            case '^': op = VersionOp::Caret; pos = 1; break;
            case '~': op = VersionOp::Tilde; pos = 1; break;
            default:  op = VersionOp::Exact; pos = 0; break;
            }

            if (pos < size) {
                target.parse(string_view(str.data() + pos, size - pos));
            }
        }

        bool satisfies(const Version version) const noexcept {
            switch (op) {
            case VersionOp::Exact:     return version == target;
            case VersionOp::Greater:   return version > target;
            case VersionOp::GreaterEq: return version >= target;
            case VersionOp::Less:      return version < target;
            case VersionOp::LessEq:    return version <= target;
            case VersionOp::Any:       return true;
            case VersionOp::Caret:     return version.Caret(target);
            case VersionOp::Tilde:     return version.Tilde(target);
            }
            return false;
        }
    };
}