#include "AdvancedLabelManager.hpp"
#include <Geode/loader/Log.hpp>
#include <Geode/utils/general.hpp>
#include <Geode/utils/string.hpp>
#include <iostream>
#include <sstream>

#include <Geode/modify/GameManager.hpp>
struct ClearCacheGMHook : geode::Modify<ClearCacheGMHook, GameManager> {
    void reloadAllStep5() {
        GameManager::reloadAllStep5();
        BMFontConfiguration::purgeCachedData();
    }
};

std::unordered_map<std::string, BMFontConfiguration>& getFontConfigs() {
    static std::unordered_map<std::string, BMFontConfiguration> s_fontConfigs;
    return s_fontConfigs;
}

BMFontConfiguration* BMFontConfiguration::create(std::string const& fntFile) {
    auto& s_fontConfigs = getFontConfigs();

    // check if the font config is already loaded
    auto it = s_fontConfigs.find(fntFile);
    if (it != s_fontConfigs.end()) {
        return &it->second;
    }

    // load the font config
    BMFontConfiguration config{};
    if (!config.initWithFNTfile(fntFile)) {
        return nullptr;
    }
    return &s_fontConfigs.emplace(fntFile, std::move(config)).first->second;
}

void BMFontConfiguration::purgeCachedData() {
    getFontConfigs().clear();
}

bool BMFontConfiguration::initWithFNTfile(std::string const& fntFile) {
    #if defined(GEODE_IS_MOBILE) || !defined(NDEBUG)
    // on android, accessing internal assets manually won't work,
    // so we're just going to use cocos functions as intended.
    // oh and fullPathForFilename apparently crashes in debug mode, so we're using this in that case as well
    unsigned long size = 0;
    auto data = cocos2d::CCFileUtils::sharedFileUtils()->getFileData(fntFile.c_str(), "rb", &size);
    if (!data || size == 0) {
        geode::log::error("Failed to read file '{}'", fntFile);
        return false;
    }
    auto contents = std::string(reinterpret_cast<char*>(data), size);
    #else
    // for non-android, we can speed up reading by doing it manually
    std::string fullPath = cocos2d::CCFileUtils::get()->fullPathForFilename(fntFile.c_str(), false);
    auto contents = geode::utils::file::readString(fullPath).unwrapOrDefault();
    if (contents.empty()) {
        geode::log::error("Failed to read file '{}'", fullPath);
        return false;
    }
    #endif

    return initWithContents(contents, fntFile);
}

#define WRAP_PARSE(expr) if (auto res = (expr); res.isErr()) { geode::log::error("{}", res.unwrapErr()); return false; }

bool BMFontConfiguration::initWithContents(std::string const& contents, std::string const& fntFile) {
    std::istringstream stream(contents);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty()) {
            continue;
        }

        std::istringstream lineStream(line);
        std::string type;
        lineStream >> type;

        if (type == "info") {
            WRAP_PARSE(parseInfoArguments(lineStream));
        } else if (type == "common") {
            WRAP_PARSE(parseCommonArguments(lineStream));
        } else if (type == "page") {
            WRAP_PARSE(parseImageFileName(lineStream, fntFile));
        } else if (type == "char") {
            WRAP_PARSE(parseCharacterDefinition(lineStream));
        } else if (type == "kerning") {
            WRAP_PARSE(parseKerningEntry(lineStream));
        }
    }

    return true;
}

template <class T>
T fastParse(std::string_view str) {
    return geode::utils::numFromString<T>(str).unwrapOrDefault();
}

geode::Result<> BMFontConfiguration::parseInfoArguments(std::istringstream& line) {
    std::string keypair;

    while (line >> keypair) {
        auto eqPos = keypair.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }

        auto key = keypair.substr(0, eqPos);
        auto value = keypair.substr(eqPos + 1);

        if (key == "padding") {
            std::istringstream paddingStream(value);
            char comma;
            paddingStream >> m_padding.left >> comma >> m_padding.top >> comma >> m_padding.right >> comma >> m_padding.bottom;
        }
    }

    return geode::Ok();
}

geode::Result<> BMFontConfiguration::parseImageFileName(std::istringstream& line, std::string const& fntFile) {
    std::string keypair;

    while (line >> keypair) {
        auto eqPos = keypair.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }

        auto key = keypair.substr(0, eqPos);
        auto value = keypair.substr(eqPos + 1);

        if (key == "file") {
            auto relPath = value.substr(1, value.size() - 2); // remove quotes
            m_atlasName = cocos2d::CCFileUtils::get()->fullPathFromRelativeFile(
                relPath.c_str(), fntFile.c_str()
            );
        }
    }

    if (m_atlasName.empty()) {
        return geode::Err("Failed to parse image file name");
    }

    return geode::Ok();
}

geode::Result<> BMFontConfiguration::parseCommonArguments(std::istringstream& line) {
    std::string keypair;

    while (line >> keypair) {
        auto eqPos = keypair.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }

        auto key = keypair.substr(0, eqPos);
        auto value = keypair.substr(eqPos + 1);

        if (key == "lineHeight") {
            m_commonHeight = fastParse<float>(value);
        } else if (key == "scaleW" || key == "scaleH") {
            if (fastParse<int>(value) > cocos2d::CCConfiguration::sharedConfiguration()->m_nMaxTextureSize) {
                return geode::Err("Font size exceeds max texture size");
            }
        } else if (key == "pages") {
            if (fastParse<int>(value) != 1) {
                return geode::Err("Font must have exactly one page");
            }
        }
    }

    return geode::Ok();
}

geode::Result<> BMFontConfiguration::parseCharacterDefinition(std::istringstream& line) {
    BMFontDef def;
    std::string keypair;

    while (line >> keypair) {
        auto eqPos = keypair.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }

        auto key = keypair.substr(0, eqPos);
        auto value = keypair.substr(eqPos + 1);

        if (key == "id") {
            def.charID = fastParse<uint32_t>(value);
        } else if (key == "x") {
            def.rect.origin.x = fastParse<int>(value);
        } else if (key == "y") {
            def.rect.origin.y = fastParse<int>(value);
        } else if (key == "width") {
            def.rect.size.width = fastParse<int>(value);
        } else if (key == "height") {
            def.rect.size.height = fastParse<int>(value);
        } else if (key == "xoffset") {
            def.xOffset = fastParse<float>(value);
        } else if (key == "yoffset") {
            def.yOffset = fastParse<float>(value);
        } else if (key == "xadvance") {
            def.xAdvance = fastParse<float>(value);
        }
    }

    m_fontDefDictionary[def.charID] = def;
    // m_characterSet.insert(def.charID);

    return geode::Ok();
}

geode::Result<> BMFontConfiguration::parseKerningEntry(std::istringstream& line) {
    std::string keypair;

    while (line >> keypair) {
        auto eqPos = keypair.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }

        auto key = keypair.substr(0, eqPos);
        auto value = keypair.substr(eqPos + 1);

        if (key == "first") {
            auto first = fastParse<uint32_t>(value);
            line >> keypair;
            eqPos = keypair.find('=');
            if (eqPos == std::string::npos) {
                return geode::Err("Failed to parse kerning entry first");
            }

            auto second = fastParse<uint32_t>(keypair.substr(eqPos + 1));
            line >> keypair;
            eqPos = keypair.find('=');
            if (eqPos == std::string::npos) {
                return geode::Err("Failed to parse kerning entry second");
            }

            auto amount = fastParse<float>(keypair.substr(eqPos + 1));
            m_kerningDictionary[{first, second}] = amount;
        }
    }

    return geode::Ok();
}

#undef WRAP_PARSE


constexpr bool isRegionalIndicator(char32_t c) {
    return c >= 0x1F1E6 && c <= 0x1F1FF;
}

constexpr static bool isEmoji(char32_t c) {
    return (c >= 0x1F300 && c <= 0x1F6FF)    // Emoticons, transport, weather
           || (c >= 0x2600 && c <= 0x27BF)   // Miscellaneous Symbols and Dingbats
           || (c >= 0x2000 && c <= 0x23FF)   // General Punctuation, Super/subscripts, Diacritical marks, etc.
           || (c >= 0x2B50 && c <= 0x2B55)   // Star emojis
           || (c >= 0x1F900 && c <= 0x1F9FF) // Supplemental Symbols and Pictographs
           || (c >= 0x1F700 && c <= 0x1F7FF) // Alchemical Symbols
           || (c >= 0x1FA00 && c <= 0x1FAFF) // Symbols and Pictographs Extended-A
           || (c >= 0x1F000 && c <= 0x1F02F) // Mahjong, Domino
           || (c >= 0xE0020 && c <= 0xE007F) // Tags for flags
           || (c >= 0x1C000 && c <= 0x1CFFF) // Custom range for special emojis
           || c == 0x20E3;                   // Combining enclosing keycap
}

constexpr static bool isSkinToneModifier(char32_t c) {
    return c >= 0x1F3FB && c <= 0x1F3FF;
}

constexpr static bool isZeroWidthJoiner(char32_t c) {
    return c == 0x200D;
}

constexpr static bool isVariationSelector(char32_t c) {
    return c >= 0xFE00 && c <= 0xFE0F;
}

constexpr static bool isDigit(char32_t c) {
    return c <= 0x0039 && c >= 0x0030;
}

constexpr static bool shouldParseDigitRegionalIndicator(std::u32string_view text) {
    return isDigit(text[0]) && text.size() > 2 && text[2] == 0x20E3 && isVariationSelector(text[1]);
}

// Custom math stuff

struct Vector {
    union {
        struct { float x, y; };
        struct { float width, height; };
    };
    constexpr Vector() noexcept : x(0.f), y(0.f) {}
    constexpr Vector(float x_, float y_) noexcept : x(x_), y(y_) {}
    constexpr operator cocos2d::CCPoint() const { return cocos2d::CCPoint { x, y }; }
    constexpr operator cocos2d::CCSize() const { return cocos2d::CCSize { x, y }; }
};

struct Rect {
    Vector origin;
    Vector size;
    constexpr Rect() noexcept = default;
    constexpr Rect(float x, float y, float w, float h) noexcept : origin(x, y), size(w, h) {}
    constexpr operator cocos2d::CCRect() const { return cocos2d::CCRect { origin, size }; }
    constexpr void setRect(float x, float y, float width, float height) {
        origin.x = x;
        origin.y = y;
        size.width = width;
        size.height = height;
    }
};

Label* Label::create(std::string_view text, std::string const& font) {
    auto ret = new Label();
    if (ret->init(text, font, BMFontAlignment::Left, 1.f)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

Label* Label::create(std::string_view text, std::string const& font, float scale) {
    auto ret = new Label();
    if (ret->init(text, font, BMFontAlignment::Left, scale)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

Label* Label::create(std::string_view text, std::string const& font, BMFontAlignment alignment) {
    auto ret = new Label();
    if (ret->init(text, font, alignment, 1.f)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

Label* Label::create(std::string_view text, std::string const& font, BMFontAlignment alignment, float scale) {
    auto ret = new Label();
    if (ret->init(text, font, alignment, scale)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

Label* Label::createWrapped(std::string_view text, std::string const& font, float wrapWidth) {
    auto ret = new Label();
    if (ret->initWrapped(text, font, BMFontAlignment::Left, 1.f, wrapWidth)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

Label* Label::createWrapped(std::string_view text, std::string const& font, float scale, float wrapWidth) {
    auto ret = new Label();
    if (ret->initWrapped(text, font, BMFontAlignment::Left, scale, wrapWidth)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

Label* Label::createWrapped(std::string_view text, std::string const& font, BMFontAlignment alignment, float wrapWidth) {
    auto ret = new Label();
    if (ret->initWrapped(text, font, alignment, 1.f, wrapWidth)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

Label* Label::createWrapped(
    std::string_view text, std::string const& font, BMFontAlignment alignment, float scale, float wrapWidth
) {
    auto ret = new Label();
    if (ret->initWrapped(text, font, alignment, scale, wrapWidth)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

void Label::setString(std::string_view text) {
    if (m_text == text) {
        return;
    }

    if (text.empty()) {
        m_unicodeText.clear();
        m_text.clear();
    } else {
        auto utf8TextRes = geode::utils::string::utf8ToUtf32(text);
        if (utf8TextRes.isErr()) {
            return;
        }

        m_unicodeText = std::move(utf8TextRes).unwrap();
        m_text = text;
    }

    updateChars();
}

void Label::setFont(std::string const& font) {
    if (m_font == font) {
        return;
    }

    auto newConfig = BMFontConfiguration::create(font);
    if (!newConfig) {
        return;
    }

    m_fontConfig = newConfig;
    m_font = font;

    m_mainBatch->setTexture(
        cocos2d::CCTextureCache::get()->addImage(
            m_fontConfig->getAtlasName().c_str(), false
        )
    );

    updateChars();
}

void Label::addFont(std::string const& font, std::optional<float> scale) {
    auto newConfig = BMFontConfiguration::create(font);
    if (!newConfig) {
        return;
    }

    // check if the font is already added
    for (auto& cfg : m_fontBatches) {
        if (cfg.config == newConfig) {
            return;
        }
    }

    // add the font
    auto batch = cocos2d::CCSpriteBatchNode::create(newConfig->getAtlasName().c_str());
    batch->setID(fmt::format("font-batch-{}", m_fontBatches.size()));
    m_fontBatches.push_back({newConfig, CachedBatch(batch), scale});
    this->addChild(batch, 0, m_fontBatches.size());
}

void Label::enableEmojis(std::string const& sheetFileName, const EmojiMap* frameNames) {
    if (m_spriteSheetBatch) {
        auto texture = cocos2d::CCTextureCache::get()->addImage(sheetFileName.c_str(), false);
        m_spriteSheetBatch->setTexture(texture);
    } else {
        m_spriteSheetBatch = cocos2d::CCSpriteBatchNode::create(sheetFileName.c_str());
        m_spriteSheetBatch->setID("emoji-sheet");
        this->addChild(m_spriteSheetBatch.node, 0, -1);
    }
    m_emojiMap = frameNames;
}

void Label::enableCustomNodes(const CustomNodeMap* nodes) {
    m_customNodeMap = nodes;
}

void Label::setWrapEnabled(bool enabled) {
    if (m_useWrap == enabled) {
        return;
    }

    m_useWrap = enabled;
    updateChars();
}

void Label::setWrapWidth(float width) {
    if (m_wrapWidth == width) {
        return;
    }

    m_wrapWidth = width;
    updateChars();
}

void Label::setWrap(bool enabled, float width) {
    m_useWrap = enabled;
    m_wrapWidth = width;
    updateChars();
}

void Label::enableEmojiColors(bool enabled) {
    m_useEmojiColors = enabled;
    updateColors();
}

void Label::setAlignment(BMFontAlignment alignment) {
    if (m_alignment == alignment) {
        return;
    }

    m_alignment = alignment;
    updateChars();
}

void Label::limitLabelWidth(float width, float defaultScale, float minScale) {
    auto originalWidth = m_obContentSize.width;
    auto scale = defaultScale;
    if (originalWidth > width && width > 0.0f) {
        scale = (width / originalWidth) * defaultScale;
    }
    if (defaultScale != 0.0f && defaultScale <= scale) {
        scale = defaultScale;
    }
    if (minScale != 0.0f && minScale >= scale) {
        scale = minScale;
    }
    this->setScale(scale);
}

void Label::addAllFonts() {
    this->addFont("font_cyrillic.fnt"_spr);
    this->addFont("font_default.fnt"_spr);
    this->addFont("font_greek.fnt"_spr);
    this->addFont("font_japanese.fnt"_spr);
    this->addFont("font_thai.fnt"_spr);
    this->addFont("font_vietnamese.fnt"_spr);
}

float Label::kerningAmountForChars(uint32_t first, uint32_t second, const BMFontConfiguration* config) {
    auto& kerningDict = config->getKerningDictionary();
    auto it = kerningDict.find({first, second});
    if (it == kerningDict.end()) {
        return 0;
    }
    return it->second;
}

void Label::hideAllChars() {
    for (auto sprite : m_sprites) {
        sprite->m_bVisible = false;
        sprite->m_bDirty = true;
    }

    for (auto custom : m_customNodes) {
        custom->removeFromParent();
    }
    m_customNodes.clear();
}

void Label::updateAlignment() const {
    if ((m_alignment == BMFontAlignment::Left || m_lines.size() < 2) && !m_useWrap) {
        return;
    }

    auto contentWidth = this->m_obContentSize.width;

    for (auto& line : m_lines) {
        if (line.empty()) {
            continue;
        }

        float offset = 0;
        if (m_alignment == BMFontAlignment::Right) {
            auto last = line.back();
            offset = contentWidth - last->m_obPosition.x - last->m_obContentSize.width * last->m_fScaleX * 0.5f;
        } else if (m_alignment == BMFontAlignment::Center) {
            auto first = line.front();
            auto last = line.back();
            auto endPos = last->m_obPosition.x + last->m_obContentSize.width * last->m_fScaleX * 0.5f;
            auto startPos = first->m_obPosition.x - first->m_obContentSize.width * first->m_fScaleX * 0.5f;
            offset = (contentWidth - endPos + startPos) * 0.5f;
        } else if (m_alignment == BMFontAlignment::Justify) {
            // TODO: justify
        }

        if (offset == 0.f) {
            continue;
        }

        for (auto& charSprite : line) {
            charSprite->setPositionX(charSprite->m_obPosition.x + offset);
        }
    }
}

float Label::getWordWidth(std::vector<cocos2d::CCSprite*> const& word) {
    if (word.empty()) {
        return 0.f;
    }

    auto first = word.front();
    auto last = word.back();

    auto firstPos = first->getPositionX();
    auto lastPos = last->getPositionX();
    auto firstSize = first->getScaledContentWidth();
    auto lastSize = last->getScaledContentWidth();

    return lastPos - firstPos + lastSize * 0.5f + firstSize * 0.5f;
}

void Label::updateCharsWrapped() {
    auto stringLen = m_unicodeText.size();
    auto textSV = std::u32string_view(m_unicodeText);

    std::vector<std::vector<std::u32string_view>> lines;
    size_t wordStart = 0;

    // split the text into lines and words
    std::vector<std::u32string_view> words;
    for (size_t i = 0; i < stringLen; ++i) {
        if (textSV[i] == ' ') {
            words.push_back(textSV.substr(wordStart, i - wordStart));
            wordStart = i + 1;
            continue;
        }

        // check if we should break the word
        if (textSV[i] == '\n') {
            words.push_back(textSV.substr(wordStart, i - wordStart));
            wordStart = i + 1;
            lines.push_back(std::move(words));
            words.clear();
        } else if (i == stringLen - 1) {
            words.push_back(textSV.substr(wordStart, i - wordStart + 1));
            lines.push_back(std::move(words));
            words.clear();
        } else if (m_breakWords > 0 && i - wordStart >= m_breakWords) {
            words.push_back(textSV.substr(wordStart, i - wordStart));
            wordStart = i;
        }
    }
    if (!words.empty()) {
        lines.push_back(std::move(words));
    }

    m_lines.clear();
    m_sprites.clear();
    m_sprites.reserve(stringLen);

    auto& mainCharset = m_fontConfig->getFontDefDictionary();
    auto commonHeight = m_fontConfig->getCommonHeight();
    auto lineHeight = commonHeight + m_extraLineSpacing;

    BMFontConfiguration* currentConfig = m_fontConfig;
    const BMFontDef* fontDef = nullptr;
    char32_t prevChar;
    float kerningAmount = 0;
    float nextX = 0;
    float longestLine = 0;
    auto scaleFactor = cocos2d::CCDirector::get()->m_fContentScaleFactor;

    auto& spaceDef = mainCharset.at(' ');
    auto spaceWidth = (m_extraKerning + spaceDef.xAdvance) / scaleFactor;

    std::vector<size_t> indices(m_fontBatches.size() + 1, 0);
    size_t emojiIndex = 0;

    struct Word {
        std::vector<CCNode*> sprites;
        float xOffset = 0.f;
        float fromX = 0;
        float toX = 0;
    };

    std::vector<std::vector<Word>> spriteLines;
    std::vector<Word> currentSpriteLine;
    Word currentSpriteWord;

    // iterate over all words and get the width of each word
    for (auto& line : lines) {
        // build the words
        for (auto& word : line) {
            auto wordLen = word.size();
            prevChar = -1;

            currentSpriteWord.fromX = nextX;

            // iterate over all characters in the word
            for (uint32_t k = 0; k < wordLen; ++k) {
                float nextY = 0;
                auto c = word[k];
                if (c == ' ') {
                    continue;
                }

                // find the font definition for the character
                float scale = 1.f;
                size_t fontIndex = 0;
                auto* currentBatch = &m_mainBatch;

                if (m_spriteSheetBatch && shouldParseDigitRegionalIndicator(word.substr(k))) {
                    checkForEmoji(
                        word, k, scaleFactor,
                        nextX, nextY, commonHeight,
                        longestLine, currentSpriteWord.sprites,
                        emojiIndex
                    );
                    continue;
                }

                fontDef = getFontDefForChar(c, m_fontConfig, scale, fontIndex, currentBatch, currentConfig);
                if (!fontDef) {
                    checkForEmoji(
                        word, k, scaleFactor,
                        nextX, nextY, commonHeight,
                        longestLine, currentSpriteWord.sprites,
                        emojiIndex
                    );
                    continue;
                }

                if (k == 0) {
                    currentSpriteWord.xOffset = (m_extraKerning + fontDef->xOffset * scale) / scaleFactor;
                }

                auto& index = indices[fontIndex];
                kerningAmount = kerningAmountForChars(prevChar, c, currentConfig) * scale;

                Rect rect = {
                    fontDef->rect.origin.x / scaleFactor, fontDef->rect.origin.y / scaleFactor,
                    fontDef->rect.size.width / scaleFactor, fontDef->rect.size.height / scaleFactor
                };

                // Re-using existing sprites for performance reasons
                auto fontChar = getSpriteForChar(*currentBatch, index, scale, rect);
                currentSpriteWord.sprites.push_back(fontChar);
                m_sprites.push_back(fontChar);

                rect.size.width *= scale;
                rect.size.height *= scale;

                float yOffset = commonHeight - fontDef->yOffset * scale;

                auto& pos = fontChar->m_obPosition;
                pos.x = (
                    nextX + fontDef->xOffset * scale + fontDef->rect.size.width * 0.5f * scale + kerningAmount
                ) / scaleFactor;
                pos.y = (
                    nextY + yOffset - rect.size.height * 0.5f * scaleFactor
                ) / scaleFactor;

                // update kerning
                auto advance = m_extraKerning + fontDef->xAdvance * scale + kerningAmount;
                nextX += advance;
                prevChar = c;

                ++index;
            }

            currentSpriteWord.toX = nextX;

            // add the word to the line
            currentSpriteLine.push_back(std::move(currentSpriteWord));
            currentSpriteWord.sprites.clear();
            currentSpriteWord.xOffset = 0;
        }

        // add the line to the lines
        spriteLines.push_back(std::move(currentSpriteLine));
        currentSpriteLine.clear();
    }

    // start wrapping the lines
    std::vector<CCNode*> currentLine;
    nextX = 0;
    auto maxWidth = m_wrapWidth / getScale();
    for (auto& spriteLine : spriteLines) {
        for (auto& word : spriteLine) {
            // auto wordWidth = getWordWidth(word.sprites);
            auto wordWidth = (word.toX - word.fromX) / scaleFactor;
            // nextX += word.xOffset;

            if (nextX + wordWidth > maxWidth) {
                // wrap the line
                m_lines.push_back(std::move(currentLine));
                currentLine.clear();
                nextX = 0;
            }

            if (!word.sprites.empty()) {
                // add the word to the line
                auto front = word.sprites.front();
                float startX = front->m_obPosition.x - front->m_obContentSize.width * front->m_fScaleX * 0.5f;
                for (auto& sprite : word.sprites) {
                    sprite->m_obPosition.x += nextX - startX;
                    currentLine.push_back(sprite);
                }
                nextX += wordWidth;
            }

            // append space
            nextX += spaceWidth;
        }

        // add the last line
        m_lines.push_back(std::move(currentLine));
        currentLine.clear();
        nextX = 0;
    }

    // recalculate Y positions
    float commonHeightScaled = (m_lines.size() <= 1 ? commonHeight : lineHeight) / scaleFactor;
    float nextY = commonHeightScaled * m_lines.size() - commonHeightScaled;
    float maxLineWidth = 0;
    for (auto& line : m_lines) {
        for (auto& sprite : line) {
            sprite->m_obPosition.y += nextY;
            maxLineWidth = std::max(maxLineWidth, sprite->m_obPosition.x + sprite->m_obContentSize.width * sprite->m_fScaleX);
        }
        nextY -= commonHeightScaled;
    }

    /// == DEBUG ==
    // for (auto& line : spriteLines) {
    //     for (auto& word : line) {
    //         // auto wordWidth = getWordWidth(word.sprites);
    //         auto wordWidth = (word.toX - word.fromX) / scaleFactor;
    //
    //         auto startX = word.sprites.front()->getPositionX() - word.sprites.front()->getScaledContentSize().width * 0.5f;
    //         auto startY = word.sprites.front()->getPositionY() - word.sprites.front()->getScaledContentSize().height * 0.5f;
    //
    //         auto spaceFiller = cocos2d::CCLayerColor::create({255, 0, 0, 100}, wordWidth, commonHeight / scaleFactor);
    //         spaceFiller->setPosition(startX, startY);
    //         this->addChild(spaceFiller, 100);
    //     }
    // }
    /// ===========

    this->setContentSize({maxLineWidth, lineHeight * m_lines.size() / scaleFactor});

    this->updateAlignment();
}

const BMFontDef* Label::getFontDefForChar(
    char32_t c, const BMFontConfiguration* config, float& outScale, size_t& outIndex,
    CachedBatch*& outBatch, BMFontConfiguration*& outConfig
) {
    auto& mainCharset = config->getFontDefDictionary();
    auto it = mainCharset.find(c);
    if (it != mainCharset.end()) {
        return &it->second;
    }

    // check for uppercase version of the character
    auto upper = std::toupper(c);
    it = mainCharset.find(upper);
    if (it != mainCharset.end()) {
        return &it->second;
    }

    // check other fonts
    for (size_t i = 0; i < m_fontBatches.size(); ++i) {
        auto& [config, batch, scale] = m_fontBatches[i];
        auto& charset = config->getFontDefDictionary();
        it = charset.find(c);
        if (it != charset.end()) {
            outIndex = i + 1;
            if (scale.has_value()) {
                // manual font scale
                outScale = scale.value();
            } else {
                // auto calculated scale
                outScale = m_fontConfig->getCommonHeight() / config->getCommonHeight();
            }
            outIndex = i + 1;
            outBatch = &batch;
            outConfig = config;
            return &it->second;
        }
    }

    return nullptr;
}

std::u32string_view Label::parseEmoji(std::u32string_view text, uint32_t& index) const {
    size_t emojiStart = index;
    size_t i = index;

    if (i >= text.size() || !(isEmoji(text[i])
                              || isRegionalIndicator(text[i])
                              || shouldParseDigitRegionalIndicator(text.substr(i)))
    ) {
        return {}; // not an emoji
    }

    // handle regional indicators
    if (isRegionalIndicator(text[i])) {
        // if the next character is also a regional indicator, check if we have it defined in the emoji map
        if (i + 1 < text.size() && isRegionalIndicator(text[i + 1]) && m_emojiMap->contains(text.substr(i, 2))) {
            ++i;
        }
    } else if (shouldParseDigitRegionalIndicator(text.substr(emojiStart))) {
        i += 2;
    } else {
        // Parse the emoji components
        while (i + 1 < text.size()) {
            // handle skin tone modifiers
            if (isSkinToneModifier(text[i + 1]) || isVariationSelector(text[i + 1])) {
                ++i;
            } else if (isZeroWidthJoiner(text[i + 1])) {
                ++i;
                if (i + 1 < text.size() && isEmoji(text[i + 1])) {
                    ++i;
                }
            } else {
                break;
            }
        }
    }

    index = i; // Update index to the end of the parsed emoji
    return text.substr(emojiStart, i - emojiStart + 1);
}

void Label::checkForEmoji(
    std::u32string_view text, uint32_t& index, float scaleFactor, float& nextX, float nextY, float commonHeight,
    float& longestLine, std::vector<CCNode*>& currentLine, size_t& emojiIndex
) {
    if (!m_spriteSheetBatch) {
        return;
    }

    auto decodedEmoji = parseEmoji(text, index);
    auto emojiIt = m_emojiMap->find(decodedEmoji);
    if (emojiIt != m_emojiMap->end()) {
        auto sprite = m_spriteSheetBatch[emojiIndex];
        if (!sprite) {
            // create new sprite
            sprite = cocos2d::CCSprite::createWithSpriteFrameName(emojiIt->second);
            if (!sprite) { return geode::log::warn("Frame {} was not found (create)", emojiIt->second); }
            m_spriteSheetBatch.addChild(sprite, emojiIndex, emojiIndex);

            // modify opacity and color
            if (m_useEmojiColors) {
                sprite->setColor(m_color);
            }
            sprite->setOpacity(m_opacity);
        } else {
            auto spriteFrame = cocos2d::CCSpriteFrameCache::get()->spriteFrameByName(emojiIt->second);
            if (!spriteFrame) { return geode::log::warn("Frame {} was not found (update)", emojiIt->second); }
            sprite->m_bVisible = true;
            sprite->setDisplayFrame(spriteFrame);
        }

        // calculate size
        auto size = sprite->getContentSize();
        auto sizeInPixels = cocos2d::CCSize{
            size.width * scaleFactor,
            size.height * scaleFactor
        };

        // rescale to fit font height
        auto sprScale = commonHeight / sizeInPixels.height;
        sprite->setScale(sprScale);
        sizeInPixels.width *= sprScale;

        // update position
        sprite->setPosition({
            (nextX + sizeInPixels.width * .5f) / scaleFactor,
            (nextY + commonHeight * .5f) / scaleFactor
        });
        nextX += sizeInPixels.width + m_extraKerning;

        // update longest line
        if (longestLine < nextX) {
            longestLine = nextX;
        }

        currentLine.push_back(sprite);
        m_sprites.push_back(sprite);
        ++emojiIndex;
    } else if (m_customNodeMap) {
        auto it = m_customNodeMap->find(decodedEmoji);
        if (it == m_customNodeMap->end()) { return; }

        auto node = it->second(text.substr(index), index);
        if (!node) { return; }

        // calculate size
        auto size = node->getContentSize();
        auto sizeInPixels = cocos2d::CCSize{
            size.width * scaleFactor,
            size.height * scaleFactor
        };

        // rescale to fit font height
        auto sprScale = commonHeight / sizeInPixels.height;
        node->setScale(sprScale);
        sizeInPixels.width *= sprScale;

        // update position
        node->setPosition({
            (nextX + sizeInPixels.width * .5f) / scaleFactor,
            (nextY + commonHeight * .5f) / scaleFactor
        });
        nextX += sizeInPixels.width + m_extraKerning;

        // update longest line
        if (longestLine < nextX) {
            longestLine = nextX;
        }

        this->addChild(node, 0, m_customNodes.size());
        currentLine.push_back(node);
        m_customNodes.push_back(node);
    }
}

cocos2d::CCSprite* Label::getSpriteForChar(
    CachedBatch& batch, size_t index, float scale, cocos2d::CCRect const& rect
) const {
    auto fontChar = batch[index];
    if (!fontChar) {
        fontChar = new cocos2d::CCSprite();

        fontChar->initWithTexture(batch->getTexture(), rect);
        fontChar->setScale(scale);
        batch.addChild(fontChar, index, index);
        fontChar->release();

        // Apply label properties
        fontChar->setOpacityModifyRGB(m_isOpacityModifyRGB);

        // Color MUST be set before opacity, since opacity might change color if OpacityModifyRGB is on
        fontChar->setColor(m_color);
        fontChar->setOpacity(m_opacity);
    } else {
        // reusing existing sprite
        fontChar->m_bVisible = true;
        fontChar->setTextureRect(rect, false, rect.size);
    }
    return fontChar;
}

void Label::updateChars() {
    hideAllChars();

    //      if (m_useChunks) {
    //          return updateChunkedChars();
    //      }

    if (m_unicodeText.empty()) {
        return this->setContentSize({0.f, 0.f});
    }

    if (m_useWrap) {
        return updateCharsWrapped();
    }

    auto stringLen = m_unicodeText.size();

    // Calculate the number of lines
    uint32_t lines = 1;
    for (uint32_t i = 0; i < stringLen; ++i) {
        if (m_unicodeText[i] == '\n') {
            ++lines;
        }
    }

    m_lines.clear();
    m_lines.reserve(lines);

    m_sprites.clear();
    m_sprites.reserve(stringLen);

    auto commonHeight = m_fontConfig->getCommonHeight();
    auto lineHeight = commonHeight + m_extraLineSpacing;

    BMFontConfiguration* currentConfig = m_fontConfig;
    const BMFontDef* fontDef = nullptr;
    char32_t prevChar = -1;
    float kerningAmount = 0;
    float nextX = 0;
    float nextY = lineHeight * lines - lineHeight;
    float longestLine = 0;
    auto scaleFactor = cocos2d::CCDirector::get()->getContentScaleFactor();

    std::vector<CCNode*> currentLine;
    std::vector<size_t> indices(m_fontBatches.size() + 1, 0);
    size_t emojiIndex = 0;

    for (uint32_t i = 0; i < stringLen; ++i) {
        char32_t c = m_unicodeText[i];

        if (c == '\n') {
            nextX = 0;
            nextY -= lineHeight;
            m_lines.push_back(std::move(currentLine));
            currentLine.clear();
            continue;
        }

        size_t fontIndex = 0;
        float scale = 1.f;
        auto* batch = &m_mainBatch;
        if (m_spriteSheetBatch && shouldParseDigitRegionalIndicator(std::u32string_view(m_unicodeText).substr(i))) {
            checkForEmoji(
                m_unicodeText, i, scaleFactor,
                nextX, nextY, commonHeight,
                longestLine, currentLine,
                emojiIndex
            );
            continue;
        }

        fontDef = getFontDefForChar(c, m_fontConfig, scale, fontIndex, batch, currentConfig);
        if (!fontDef) {
            checkForEmoji(
                m_unicodeText, i, scaleFactor,
                nextX, nextY, commonHeight,
                longestLine, currentLine,
                emojiIndex
            );
            continue;
        }

        auto& index = indices[fontIndex];
        kerningAmount = kerningAmountForChars(prevChar, c, currentConfig) * scale;

        Rect rect = {
            fontDef->rect.origin.x / scaleFactor, fontDef->rect.origin.y / scaleFactor,
            fontDef->rect.size.width / scaleFactor, fontDef->rect.size.height / scaleFactor
        };

        // Re-using existing sprites for performance reasons
        auto fontChar = getSpriteForChar(*batch, index, scale, rect);
        currentLine.push_back(fontChar);
        m_sprites.push_back(fontChar);

        rect.size.width *= scale;
        rect.size.height *= scale;

        float yOffset = commonHeight - fontDef->yOffset * scale;
        Vector fontPos = {
            nextX + fontDef->xOffset * scale + fontDef->rect.size.width * 0.5f * scale + kerningAmount,
            nextY + yOffset - rect.size.height * 0.5f * scaleFactor
        };
        fontChar->setPosition({
            fontPos.x / scaleFactor,
            fontPos.y / scaleFactor
        });

        // update kerning
        nextX += m_extraKerning + fontDef->xAdvance * scale + kerningAmount;
        prevChar = c;

        longestLine = std::max(longestLine, nextX);
        ++index;
    }
    m_lines.push_back(std::move(currentLine));

    float width = longestLine;
    if (fontDef && fontDef->xAdvance < fontDef->rect.size.width) {
        width += fontDef->rect.size.width - fontDef->xAdvance;
    }

    this->setContentSize({
        width / scaleFactor,
        (commonHeight * lines + m_extraLineSpacing * (lines - 1)) / scaleFactor
    });

    this->updateAlignment();
}

void Label::updateColors() const {
    for (auto sprite : m_sprites) {
        if (m_useEmojiColors) {
            sprite->setColor(m_color);
            continue;
        }

        if (sprite->m_pParent == m_spriteSheetBatch.node) {
            sprite->setColor(cocos2d::ccc3(255, 255, 255));
        } else {
            sprite->setColor(m_color);
        }
    }
}

void Label::updateOpacity() const {
    for (auto sprite : m_sprites) {
        sprite->setOpacity(m_opacity);
    }
}

bool Label::init(std::string_view text, std::string const& font, BMFontAlignment alignment, float scale) {
    m_fontConfig = BMFontConfiguration::create(font);
    if (!m_fontConfig) {
        return false;
    }

    m_mainBatch = cocos2d::CCSpriteBatchNode::create(m_fontConfig->getAtlasName().c_str());
    if (!m_mainBatch) {
        return false;
    }

    m_font = font;
    m_alignment = alignment;

    m_mainBatch->setID("main-batch");
    this->setScale(scale);
    this->addChild(m_mainBatch.node, 0, 0);

    this->setAnchorPoint({0.5f, 0.5f});
    this->setString(text);

    return true;
}

bool Label::initWrapped(
    std::string_view text, std::string const& font, BMFontAlignment alignment, float scale, float wrapWidth
) {
    m_useWrap = true;
    m_wrapWidth = wrapWidth;
    return init(text, font, alignment, scale);
}