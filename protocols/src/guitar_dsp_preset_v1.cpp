#include <dsp/protocols/guitar_dsp_preset_v1.hpp>

#include <algorithm>
#include <charconv>
#include <system_error>

namespace dsp::protocols {
namespace {

constexpr std::string_view schema_open = R"({"schema":")";
constexpr std::string_view version_open = R"(,"version":)";
constexpr std::string_view preset_open = R"(,"preset":")";
constexpr std::string_view schema_prefix =
    R"({"schema":"guitar-dsp.preset","version":1,"preset":")";
constexpr std::string_view tempo_prefix = R"(,"tempo_bpm":)";
constexpr std::string_view parsed_tempo_prefix = R"(,"tempo_bpm":)";
constexpr std::string_view parameters_prefix =
    R"(,"parameters":{"delay.feedback_milli":)";
constexpr std::string_view free_prefix = R"(,"delay.free_ms":)";
constexpr std::string_view mix_prefix = R"(,"delay.mix_milli":)";
constexpr std::string_view subdivision_prefix =
    R"(,"delay.subdivision":")";
constexpr std::string_view suffix = R"("}})";

bool valid_preset_name(std::string_view value) noexcept {
    if (value.empty() || value.size() > 32U) return false;
    if (value.front() == '-' || value.back() == '-') return false;

    bool previous_hyphen = false;
    for (const char character : value) {
        const bool alpha =
            character >= 'a' && character <= 'z';
        const bool digit =
            character >= '0' && character <= '9';
        if (alpha || digit) {
            previous_hyphen = false;
            continue;
        }
        if (character != '-' || previous_hyphen) return false;
        previous_hyphen = true;
    }
    return true;
}

std::string_view subdivision_name(PresetSubdivisionV1 value) noexcept {
    switch (value) {
        case PresetSubdivisionV1::free:
            return "free";
        case PresetSubdivisionV1::quarter:
            return "quarter";
        case PresetSubdivisionV1::dotted_eighth:
            return "dotted-eighth";
        case PresetSubdivisionV1::eighth:
            return "eighth";
        case PresetSubdivisionV1::eighth_triplet:
            return "eighth-triplet";
        case PresetSubdivisionV1::sixteenth:
            return "sixteenth";
    }
    return {};
}

ProtocolError validate_impl(const DelayPresetV1& preset) noexcept {
    if (!valid_preset_name(preset.preset_name())) {
        return ProtocolError::invalid_preset;
    }
    if (preset.tempo_bpm < 20U || preset.tempo_bpm > 300U) {
        return ProtocolError::invalid_tempo;
    }
    if (preset.feedback_milli > 950U) {
        return ProtocolError::invalid_feedback;
    }
    if (preset.free_ms < 1U || preset.free_ms > 2000U) {
        return ProtocolError::invalid_free_ms;
    }
    if (preset.mix_milli > 1000U) {
        return ProtocolError::invalid_mix;
    }
    if (subdivision_name(preset.subdivision).empty()) {
        return ProtocolError::invalid_subdivision;
    }
    return ProtocolError::none;
}

class Cursor {
public:
    explicit Cursor(std::string_view input) noexcept : input_{input} {}

    bool consume(std::string_view literal) noexcept {
        if (literal.size() > input_.size() - position_) return false;
        if (input_.substr(position_, literal.size()) != literal) return false;
        position_ += literal.size();
        return true;
    }

    bool quoted(std::string_view& value) noexcept {
        const std::size_t start = position_;
        while (position_ < input_.size()) {
            const char current = input_[position_];
            if (current == '"') {
                value = input_.substr(start, position_ - start);
                ++position_;
                return true;
            }
            if (current == '\\' ||
                static_cast<unsigned char>(current) < 0x20U) {
                return false;
            }
            ++position_;
        }
        return false;
    }

    bool unsigned_integer(std::uint32_t& value) noexcept {
        const std::size_t start = position_;
        while (position_ < input_.size() &&
               input_[position_] >= '0' &&
               input_[position_] <= '9') {
            ++position_;
        }
        if (position_ == start) return false;
        const auto digits = input_.substr(start, position_ - start);
        const auto result =
            std::from_chars(digits.data(), digits.data() + digits.size(), value);
        return result.ec == std::errc{} &&
               result.ptr == digits.data() + digits.size();
    }

    [[nodiscard]] bool at_end() const noexcept {
        return position_ == input_.size();
    }

private:
    std::string_view input_;
    std::size_t position_{};
};

class Writer {
public:
    explicit Writer(std::span<char> output) noexcept : output_{output} {}

    bool append(std::string_view value) noexcept {
        if (value.size() > output_.size() - position_) return false;
        std::copy(value.begin(), value.end(), output_.begin() + position_);
        position_ += value.size();
        return true;
    }

    bool append_uint(std::uint16_t value) noexcept {
        if (position_ >= output_.size()) return false;
        char* const first = output_.data() + position_;
        char* const last = output_.data() + output_.size();
        const auto result = std::to_chars(first, last, value);
        if (result.ec != std::errc{}) return false;
        position_ = static_cast<std::size_t>(result.ptr - output_.data());
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept { return position_; }

private:
    std::span<char> output_;
    std::size_t position_{};
};

}  // namespace

ProtocolError set_preset_name(
    DelayPresetV1& preset,
    std::string_view name) noexcept {
    if (!valid_preset_name(name)) return ProtocolError::invalid_preset;

    preset.preset.fill('\0');
    std::copy(name.begin(), name.end(), preset.preset.begin());
    preset.preset_length = static_cast<std::uint8_t>(name.size());
    return ProtocolError::none;
}

ProtocolError validate_preset_v1(
    const DelayPresetV1& preset) noexcept {
    return validate_impl(preset);
}

SerializeResult serialize_preset_v1(
    const DelayPresetV1& preset,
    std::span<char> output) noexcept {
    const ProtocolError error = validate_preset_v1(preset);
    if (error != ProtocolError::none) return {error, 0U};

    Writer writer{output};
    const auto subdivision = subdivision_name(preset.subdivision);
    const bool complete =
        writer.append(schema_prefix) &&
        writer.append(preset.preset_name()) &&
        writer.append(tempo_prefix) &&
        writer.append_uint(preset.tempo_bpm) &&
        writer.append(parameters_prefix) &&
        writer.append_uint(preset.feedback_milli) &&
        writer.append(free_prefix) &&
        writer.append_uint(preset.free_ms) &&
        writer.append(mix_prefix) &&
        writer.append_uint(preset.mix_milli) &&
        writer.append(subdivision_prefix) &&
        writer.append(subdivision) &&
        writer.append(suffix);

    if (!complete) return {ProtocolError::too_large, 0U};
    return {ProtocolError::none, writer.size()};
}

ParseResult parse_preset_v1(std::string_view input) noexcept {
    if (input.size() > 256U) return {ProtocolError::too_large, {}};

    Cursor cursor{input};
    std::string_view token;
    std::uint32_t integer{};

    if (!cursor.consume(schema_open) || !cursor.quoted(token)) {
        return {ProtocolError::non_canonical, {}};
    }
    if (token != "guitar-dsp.preset") {
        return {ProtocolError::wrong_schema, {}};
    }

    if (!cursor.consume(version_open) || !cursor.unsigned_integer(integer)) {
        return {ProtocolError::non_canonical, {}};
    }
    if (integer != 1U) {
        return {ProtocolError::unsupported_version, {}};
    }

    DelayPresetV1 preset;
    if (!cursor.consume(preset_open) || !cursor.quoted(token)) {
        return {ProtocolError::non_canonical, {}};
    }
    if (const auto error = set_preset_name(preset, token);
        error != ProtocolError::none) {
        return {error, {}};
    }

    if (!cursor.consume(parsed_tempo_prefix) || !cursor.unsigned_integer(integer)) {
        return {ProtocolError::non_canonical, {}};
    }
    if (integer < 20U || integer > 300U) {
        return {ProtocolError::invalid_tempo, {}};
    }
    preset.tempo_bpm = static_cast<std::uint16_t>(integer);

    if (!cursor.consume(parameters_prefix) ||
        !cursor.unsigned_integer(integer)) {
        return {ProtocolError::non_canonical, {}};
    }
    if (integer > 950U) {
        return {ProtocolError::invalid_feedback, {}};
    }
    preset.feedback_milli = static_cast<std::uint16_t>(integer);

    if (!cursor.consume(free_prefix) || !cursor.unsigned_integer(integer)) {
        return {ProtocolError::non_canonical, {}};
    }
    if (integer < 1U || integer > 2000U) {
        return {ProtocolError::invalid_free_ms, {}};
    }
    preset.free_ms = static_cast<std::uint16_t>(integer);

    if (!cursor.consume(mix_prefix) || !cursor.unsigned_integer(integer)) {
        return {ProtocolError::non_canonical, {}};
    }
    if (integer > 1000U) {
        return {ProtocolError::invalid_mix, {}};
    }
    preset.mix_milli = static_cast<std::uint16_t>(integer);

    if (!cursor.consume(subdivision_prefix) || !cursor.quoted(token) ||
        !cursor.consume("}}") || !cursor.at_end()) {
        return {ProtocolError::non_canonical, {}};
    }

    if (token == "free") {
        preset.subdivision = PresetSubdivisionV1::free;
    } else if (token == "quarter") {
        preset.subdivision = PresetSubdivisionV1::quarter;
    } else if (token == "dotted-eighth") {
        preset.subdivision = PresetSubdivisionV1::dotted_eighth;
    } else if (token == "eighth") {
        preset.subdivision = PresetSubdivisionV1::eighth;
    } else if (token == "eighth-triplet") {
        preset.subdivision = PresetSubdivisionV1::eighth_triplet;
    } else if (token == "sixteenth") {
        preset.subdivision = PresetSubdivisionV1::sixteenth;
    } else {
        return {ProtocolError::invalid_subdivision, {}};
    }

    std::array<char, 256> canonical{};
    const auto encoded = serialize_preset_v1(preset, canonical);
    if (!encoded ||
        std::string_view{canonical.data(), encoded.size} != input) {
        return {ProtocolError::non_canonical, {}};
    }
    return {ProtocolError::none, preset};
}

}  // namespace dsp::protocols
