// SPDX-License-Identifier: Apache-2.0
#include <text_shaper/coretext_locator.h>
#include <text_shaper/directwrite_locator.h>
#include <text_shaper/font_locator_provider.h>
#include <text_shaper/fontconfig_locator.h>
#include <text_shaper/mock_font_locator.h>

#include <memory>

namespace text
{

using std::make_unique;

font_locator_provider& font_locator_provider::get()
{
    auto static instance = font_locator_provider {};
    return instance;
}

#if defined(__APPLE__)
font_locator& font_locator_provider::coretext()
{
    if (!_coretext)
        _coretext = make_unique<coretext_locator>();

    return *_coretext;
}
#endif

#if defined(_WIN32)
font_locator& font_locator_provider::directwrite()
{
    if (!_directwrite)
        _directwrite = make_unique<directwrite_locator>();

    return *_directwrite;
}
#endif

#if !defined(_WIN32)
font_locator& font_locator_provider::fontconfig()
{
    if (!_fontconfig)
        _fontconfig = make_unique<fontconfig_locator>();

    return *_fontconfig;
}
#endif

font_locator& font_locator_provider::mock()
{
    if (!_mock)
        _mock = make_unique<mock_font_locator>();

    return *_mock;
}

} // namespace text
