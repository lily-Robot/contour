// SPDX-License-Identifier: Apache-2.0
#include <vtparser/Parser.h>
#include <vtparser/ParserEvents.h>

#include <crispy/escape.h>

#include <libunicode/convert.h>

#include <catch2/catch_test_macros.hpp>

using namespace std;

class MockParserEvents final: public vtparser::NullParserEvents
{
  public:
    std::string text;
    std::string apc;
    std::string pm;
    size_t maxCharCount = 80;

    void error(string_view const& msg) override { INFO(fmt::format("Parser error received. {}", msg)); }

    void print(char32_t ch) override
    {
        UNSCOPED_INFO(fmt::format("print: U+{:X}", (unsigned) ch));
        text += unicode::convert_to<char>(ch);
    }
    size_t print(std::string_view s, size_t cellCount) override
    {
        UNSCOPED_INFO(fmt::format("print: {}", crispy::escape(s)));
        text += s;
        return maxCharCount -= cellCount;
    }

    void startAPC() override { apc += "{"; }
    void putAPC(char ch) override { apc += ch; }
    void dispatchAPC() override { apc += "}"; }

    void startPM() override { pm += "{"; }
    void putPM(char ch) override { pm += ch; }
    void dispatchPM() override { pm += "}"; }
};

TEST_CASE("Parser.utf8_single", "[Parser]")
{
    MockParserEvents textListener;
    auto p = vtparser::Parser<vtparser::ParserEvents>(textListener);

    p.parseFragment("\xC3\xB6"); // ö

    REQUIRE(textListener.text == "\xC3\xB6");
}

TEST_CASE("Parser.PM")
{
    MockParserEvents listener;
    auto p = vtparser::Parser<vtparser::ParserEvents>(listener);
    REQUIRE(p.state() == vtparser::State::Ground);
    // Also include ✅ in the payload to ensure such codepoints work, too.
    p.parseFragment("ABC\033^hello ✅ world\033\\DEF"sv);
    CHECK(p.state() == vtparser::State::Ground);
    CHECK(listener.pm == "{hello ✅ world}");
    CHECK(listener.text == "ABCDEF");
}

TEST_CASE("Parser.APC")
{
    MockParserEvents listener;
    auto p = vtparser::Parser<vtparser::ParserEvents>(listener);
    REQUIRE(p.state() == vtparser::State::Ground);
    p.parseFragment("ABC\033\\\033_Gi=1,a=q;\033\\DEF"sv);
    REQUIRE(p.state() == vtparser::State::Ground);
    REQUIRE(listener.apc == "{Gi=1,a=q;}");
    REQUIRE(listener.text == "ABCDEF");
}
