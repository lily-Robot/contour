// SPDX-License-Identifier: Apache-2.0
#include <vtbackend/MockTerm.h>
#include <vtbackend/Screen.h>
#include <vtbackend/Selector.h>

#include <catch2/catch_test_macros.hpp>

using crispy::size;
using namespace std;
using namespace std::placeholders;
using namespace vtbackend;

namespace
{

template <typename T>
struct TestSelectionHelper: public vtbackend::SelectionHelper
{
    Screen<T>* screen;
    explicit TestSelectionHelper(Screen<T>& self): screen { &self } {}

    [[nodiscard]] PageSize pageSize() const noexcept override { return screen->pageSize(); }
    [[nodiscard]] bool wrappedLine(LineOffset line) const noexcept override
    {
        return screen->isLineWrapped(line);
    }
    [[nodiscard]] bool cellEmpty(CellLocation pos) const noexcept override { return screen->at(pos).empty(); }
    [[nodiscard]] int cellWidth(CellLocation pos) const noexcept override { return screen->at(pos).width(); }
};

template <typename T>
TestSelectionHelper(Screen<T>&) -> TestSelectionHelper<T>;

} // namespace

// Different cases to test
// - single cell
// - inside single line
// - multiple lines
// - multiple lines fully in history
// - multiple lines from history into main buffer
// all of the above with and without scrollback != 0.

namespace
{
template <typename T>
[[maybe_unused]] void logScreenTextAlways(Screen<T> const& screen, string const& headline = "")
{
    fmt::print("{}: ZI={} cursor={} HM={}..{}\n",
               headline.empty() ? "screen dump"s : headline,
               screen.grid().zero_index(),
               screen.realCursorPosition(),
               screen.margin().horizontal.from,
               screen.margin().horizontal.to);
    fmt::print("{}\n", dumpGrid(screen.grid()));
}

template <typename T>
struct TextSelection
{
    Screen<T> const* screen;
    string text;
    ColumnOffset lastColumn = ColumnOffset(0);

    explicit TextSelection(Screen<T> const& s): screen { &s } {}

    void operator()(CellLocation const& pos)
    {
        auto const& cell = screen->at(pos);
        text += pos.column < lastColumn ? "\n" : "";
        text += cell.toUtf8();
        lastColumn = pos.column;
    }
};
template <typename T>
TextSelection(Screen<T> const&) -> TextSelection<T>;
} // namespace

TEST_CASE("Selector.Linear", "[selector]")
{
    auto screenEvents = ScreenEvents {};
    auto term = MockTerm(PageSize { LineCount(3), ColumnCount(11) }, LineCount(5));
    auto& screen = term.terminal.primaryScreen();
    auto selectionHelper = TestSelectionHelper(screen);
    term.writeToScreen(
        //       0123456789A
        /* 0 */ "12345,67890"s +
        /* 1 */ "ab,cdefg,hi"s +
        /* 2 */ "12345,67890"s);

    REQUIRE(screen.grid().lineText(LineOffset(0)) == "12345,67890");
    REQUIRE(screen.grid().lineText(LineOffset(1)) == "ab,cdefg,hi");
    REQUIRE(screen.grid().lineText(LineOffset(2)) == "12345,67890");

    // SECTION("single-cell")
    // { // "b"
    //     auto const pos = CellLocation { LineOffset(1), ColumnOffset(1) };
    //     auto selector = LinearSelection(selectionHelper, pos, []() {});
    //     (void) selector.extend(pos);
    //     selector.complete();
    //
    //     vector<Selection::Range> const selection = selector.ranges();
    //     REQUIRE(selection.size() == 1);
    //     Selection::Range const& r1 = selection[0];
    //     CHECK(r1.line == pos.line);
    //     CHECK(r1.fromColumn == pos.column);
    //     CHECK(r1.toColumn == pos.column);
    //     CHECK(r1.length() == ColumnCount(1));
    //
    //     auto selectedText = TextSelection { screen };
    //     renderSelection(selector, selectedText);
    //     CHECK(selectedText.text == "b");
    // }

    // SECTION("forward single-line")
    // { // "b,c"
    //     auto const pos = CellLocation { LineOffset(1), ColumnOffset(1) };
    //     auto selector = LinearSelection(selectionHelper, pos, []() {});
    //     (void) selector.extend(CellLocation { LineOffset(1), ColumnOffset(3) });
    //     selector.complete();
    //
    //     vector<Selection::Range> const selection = selector.ranges();
    //     REQUIRE(selection.size() == 1);
    //     Selection::Range const& r1 = selection[0];
    //     CHECK(r1.line == LineOffset(1));
    //     CHECK(r1.fromColumn == ColumnOffset(1));
    //     CHECK(r1.toColumn == ColumnOffset(3));
    //     CHECK(r1.length() == ColumnCount(3));
    //
    //     auto selectedText = TextSelection { screen };
    //     renderSelection(selector, selectedText);
    //     CHECK(selectedText.text == "b,c");
    // }

    // SECTION("forward multi-line")
    // { // "b,cdefg,hi\n1234"
    //     auto const pos = CellLocation { LineOffset(1), ColumnOffset(1) };
    //     auto selector = LinearSelection(selectionHelper, pos, []() {});
    //     (void) selector.extend(CellLocation { LineOffset(2), ColumnOffset(3) });
    //     selector.complete();
    //
    //     vector<Selection::Range> const selection = selector.ranges();
    //     REQUIRE(selection.size() == 2);
    //
    //     Selection::Range const& r1 = selection[0];
    //     CHECK(r1.line == LineOffset(1));
    //     CHECK(r1.fromColumn == ColumnOffset(1));
    //     CHECK(r1.toColumn == ColumnOffset(10));
    //     CHECK(r1.length() == ColumnCount(10));
    //
    //     Selection::Range const& r2 = selection[1];
    //     CHECK(r2.line == LineOffset(2));
    //     CHECK(r2.fromColumn == ColumnOffset(0));
    //     CHECK(r2.toColumn == ColumnOffset(3));
    //     CHECK(r2.length() == ColumnCount(4));
    //
    //     auto selectedText = TextSelection { screen };
    //     renderSelection(selector, selectedText);
    //     CHECK(selectedText.text == "b,cdefg,hi\n1234");
    // }

    // SECTION("multiple lines fully in history")
    {
        term.writeToScreen("foo\r\nbar\r\n"); // move first two lines into history.
        /*
         * |  0123456789A
        -2 | "12345,67890"
        -1 | "ab,cdefg,hi"       [fg,hi]
         0 | "12345,67890"       [123]
         1 | "foo"
         2 | "bar"
        */

        auto selector =
            LinearSelection(selectionHelper, CellLocation { LineOffset(-2), ColumnOffset(6) }, []() {});
        (void) selector.extend(CellLocation { LineOffset(-1), ColumnOffset(2) });
        selector.complete();

        vector<Selection::Range> const selection = selector.ranges();
        REQUIRE(selection.size() == 2);

        Selection::Range const& r1 = selection[0];
        CHECK(r1.line == LineOffset(-2));
        CHECK(r1.fromColumn == ColumnOffset(6));
        CHECK(r1.toColumn == ColumnOffset(10));
        CHECK(r1.length() == ColumnCount(5));

        Selection::Range const& r2 = selection[1];
        CHECK(r2.line == LineOffset(-1));
        CHECK(r2.fromColumn == ColumnOffset(0));
        CHECK(r2.toColumn == ColumnOffset(2));
        CHECK(r2.length() == ColumnCount(3));

        auto selectedText = TextSelection { screen };
        renderSelection(selector, selectedText);
        CHECK(selectedText.text == "fg,hi\n123");
    }

    // SECTION("multiple lines from history into main buffer")
    // {
    //     term.writeToScreen("foo\r\nbar\r\n"); // move first two lines into history.
    //     /*
    //     -3 | "12345,67890"
    //     -2 | "ab,cdefg,hi"         (--
    //     -1 | "12345,67890" -----------
    //      0 | "foo"         --)
    //      1 | "bar"
    //      2 | ""
    //     */
    //
    //     auto selector =
    //         LinearSelection(selectionHelper, CellLocation { LineOffset(-2), ColumnOffset(8) }, []() {});
    //     (void) selector.extend(CellLocation { LineOffset(0), ColumnOffset(1) });
    //     selector.complete();
    //
    //     vector<Selection::Range> const selection = selector.ranges();
    //     REQUIRE(selection.size() == 3);
    //
    //     Selection::Range const& r1 = selection[0];
    //     CHECK(r1.line == LineOffset(-2));
    //     CHECK(r1.fromColumn == ColumnOffset(8));
    //     CHECK(r1.toColumn == ColumnOffset(10));
    //     CHECK(r1.length() == ColumnCount(3));
    //
    //     Selection::Range const& r2 = selection[1];
    //     CHECK(r2.line == LineOffset(-1));
    //     CHECK(r2.fromColumn == ColumnOffset(0));
    //     CHECK(r2.toColumn == ColumnOffset(10));
    //     CHECK(r2.length() == ColumnCount(11));
    //
    //     Selection::Range const& r3 = selection[2];
    //     CHECK(r3.line == LineOffset(0));
    //     CHECK(r3.fromColumn == ColumnOffset(0));
    //     CHECK(r3.toColumn == ColumnOffset(1));
    //     CHECK(r3.length() == ColumnCount(2));
    //
    //     auto selectedText = TextSelection { screen };
    //     renderSelection(selector, selectedText);
    //     CHECK(selectedText.text == ",hi\n12345,67890\nfo");
    // }
}

TEST_CASE("Selector.LinearWordWise", "[selector]")
{
    // TODO
}

TEST_CASE("Selector.FullLine", "[selector]")
{
    // TODO
}

TEST_CASE("Selector.Rectangular", "[selector]")
{
    // TODO
}
