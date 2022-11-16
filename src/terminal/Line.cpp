/**
 * This file is part of the "libterminal" project
 *   Copyright (c) 2019-2020 Christian Parpart <christian@parpart.family>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <terminal/GraphicsAttributes.h>
#include <terminal/Line.h>
#include <terminal/primitives.h>

#include <unicode/grapheme_segmenter.h>
#include <unicode/utf8.h>
#include <unicode/width.h>

using std::get;
using std::holds_alternative;
using std::min;

namespace terminal
{

template <typename Cell>
typename Line<Cell>::InflatedBuffer Line<Cell>::reflow(ColumnCount _newColumnCount)
{
    using crispy::Comparison;
    if (isTrivialBuffer())
    {
        switch (crispy::strongCompare(_newColumnCount, ColumnCount::cast_from(trivialBuffer().text.size())))
        {
            case Comparison::Greater: trivialBuffer().displayWidth = _newColumnCount; return {};
            case Comparison::Equal: return {};
            case Comparison::Less:;
        }
    }
    auto& buffer = inflatedBuffer();
    // TODO: Efficiently handle TrivialBuffer-case.
    switch (crispy::strongCompare(_newColumnCount, size()))
    {
        case Comparison::Equal: break;
        case Comparison::Greater: buffer.resize(unbox<size_t>(_newColumnCount)); break;
        case Comparison::Less: {
            // TODO: properly handle wide character cells
            // - when cutting in the middle of a wide char, the wide char gets wrapped and an empty
            //   cell needs to be injected to match the expected column width.

            if (wrappable())
            {
                auto const [reflowStart, reflowEnd] = [_newColumnCount, &buffer]() {
                    auto const reflowStart =
                        next(buffer.begin(), *_newColumnCount /* - buffer[_newColumnCount].width()*/);

                    auto reflowEnd = buffer.end();

                    while (reflowEnd != reflowStart && prev(reflowEnd)->empty())
                        reflowEnd = prev(reflowEnd);

                    return std::tuple { reflowStart, reflowEnd };
                }();

                auto removedColumns = InflatedBuffer(reflowStart, reflowEnd);
                buffer.erase(reflowStart, buffer.end());
                assert(size() == _newColumnCount);
#if 0
                if (removedColumns.size() > 0 &&
                        std::any_of(removedColumns.begin(), removedColumns.end(),
                            [](Cell const& x)
                            {
                                if (!x.empty())
                                    fmt::print("non-empty cell in reflow: {}\n", x.toUtf8());
                                return !x.empty();
                            }))
                    printf("Wrapping around\n");
#endif
                return removedColumns;
            }
            else
            {
                buffer.resize(unbox<size_t>(_newColumnCount));
                assert(size() == _newColumnCount);
                return {};
            }
        }
    }
    return {};
}

template <typename Cell>
inline void Line<Cell>::resize(ColumnCount _count)
{
    assert(*_count >= 0);
#if 0 // TODO(pr)
    if (1) // constexpr (Optimized)
    {
        if (isTrivialBuffer())
        {
            TrivialBuffer& buffer = trivialBuffer();
            buffer.displayWidth = _count;
            return;
        }
    }
    inflatedBuffer().resize(unbox<size_t>(_count));
#endif
}

template <typename Cell>
gsl::span<Cell const> Line<Cell>::trim_blank_right() const noexcept
{
    auto i = inflatedBuffer().data();
    auto e = inflatedBuffer().data() + inflatedBuffer().size();

    while (i != e && (e - 1)->empty())
        --e;

    return gsl::make_span(i, e);
}

template <typename Cell>
std::string Line<Cell>::toUtf8() const
{
    if (isTrivialBuffer())
    {
        auto const& lineBuffer = trivialBuffer();
        auto str = std::string(lineBuffer.text.data(), lineBuffer.text.size());
        for (auto i = lineBuffer.usedColumns; i < lineBuffer.displayWidth; ++i)
            str += ' ';
        return str;
    }

    std::string str;
    for (Cell const& cell: inflatedBuffer())
    {
        if (cell.codepointCount() == 0)
            str += ' ';
        else
            str += cell.toUtf8();
    }
    return str;
}

template <typename Cell>
std::string Line<Cell>::toUtf8Trimmed() const
{
    std::string output = toUtf8();
    while (!output.empty() && isspace(output.back()))
        output.pop_back();

    size_t frontGap = 0;
    while (frontGap < output.size() && std::isspace(output[frontGap]))
        frontGap++;
    output = output.substr(frontGap);

    return output;
}

template <typename Cell>
InflatedLineBuffer<Cell> inflate(TrivialLineBuffer const& input, crispy::BufferObject<Cell>& cellPool)
{
    static constexpr char32_t ReplacementCharacter { 0xFFFD };

    auto columns = InflatedLineBuffer<Cell> { cellPool.advance(input.displayWidth) };
    columns.reserve(unbox<size_t>(input.displayWidth));

    auto lastChar = char32_t { 0 };
    auto utf8DecoderState = unicode::utf8_decoder_state {};
    auto gapPending = 0;

    for (char const ch: input.text.view())
    {
        unicode::ConvertResult const r = unicode::from_utf8(utf8DecoderState, static_cast<uint8_t>(ch));
        if (holds_alternative<unicode::Incomplete>(r))
            continue;

        auto const nextChar =
            holds_alternative<unicode::Success>(r) ? get<unicode::Success>(r).value : ReplacementCharacter;

        if (unicode::grapheme_segmenter::breakable(lastChar, nextChar))
        {
            while (gapPending > 0)
            {
                columns.emplace_back(Cell { input.textAttributes, input.hyperlink });
                --gapPending;
            }
            auto const charWidth = unicode::width(nextChar);
            columns.emplace_back(Cell {});
            columns.back().setHyperlink(input.hyperlink);
            columns.back().write(input.textAttributes, nextChar, static_cast<uint8_t>(charWidth));
            gapPending = charWidth - 1;
        }
        else
        {
            Cell& prevCell = columns.back();
            auto const extendedWidth = prevCell.appendCharacter(nextChar);
            if (extendedWidth > 0)
            {
                auto const cellsAvailable = *input.displayWidth - static_cast<int>(columns.size()) + 1;
                auto const n = min(extendedWidth, cellsAvailable);
                for (int i = 1; i < n; ++i)
                {
                    columns.emplace_back(Cell { input.textAttributes });
                    columns.back().setHyperlink(input.hyperlink);
                }
            }
        }
        lastChar = nextChar;
    }

    while (gapPending > 0)
    {
        columns.emplace_back(Cell { input.textAttributes, input.hyperlink });
        --gapPending;
    }

    assert(columns.size() == unbox<size_t>(input.usedColumns));

    while (columns.size() < unbox<size_t>(input.displayWidth))
        columns.emplace_back(Cell { input.fillAttributes });

    return columns;
}
} // end namespace terminal

#include <terminal/cell/CompactCell.h>
template class terminal::Line<terminal::CompactCell>;

#include <terminal/cell/SimpleCell.h>
template class terminal::Line<terminal::SimpleCell>;
