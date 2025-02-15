#include "tokenizer.h"

#include "fileutils.h"

#include <locale>
#include <codecvt>
namespace TinyTokenizer {

boost::regex pattern("('s|'t|'re|'ve|'m|'ll|'d| ?[A-Za-z]+| ?[0-9]+| ?[^\\sA-Za-z0-9]+|\\s+\\(?!\\S\\)|\\s+)");


std::wstring StringUtils::utf82wstring(const std::string &str) {
    static std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.from_bytes(str);
}

std::string StringUtils::wstring2utf8(const std::wstring &str) {
    static std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(str);
}

std::vector<std::wstring> StringUtils::split(const std::wstring &s, wchar_t delim) {
    std::wstringstream ss(s);
    std::wstring item;
    std::vector<std::wstring> elems;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

Encoder::Encoder(const std::unordered_map<std::wstring, int32_t> &encoder,
                 const std::vector<wstring_pair> &bpeMerges)
    : Encoder() {
    encoder_ = encoder;
    for (auto &kv : encoder_) {
        decoder_.insert({kv.second, kv.first});
    }
    byteEncoder_ = bytesToUnicode();
    for (auto &kv : byteEncoder_) {
        byteDecoder_.insert({kv.second, kv.first});
    }
    for (size_t idx = 0; idx < bpeMerges.size(); idx++) {
        bpeRanks_.insert({bpeMerges[idx], (int32_t) idx});
    }
    cache_.clear();
}

std::wstring Encoder::bpe(const std::wstring &token) {
    auto cacheItem = cache_.find(token);
    if (cacheItem != cache_.end()) {
        return cacheItem->second;
    }

    std::vector<std::wstring> word;
    for (auto &c : token) {
        word.emplace_back(1, c);
    }

    auto pairs = getPairs(word);
    if (pairs.empty()) {
        return token;
    }

    while (true) {
        wstring_pair bigram;
        size_t minRank = SIZE_MAX;
        for (auto &pair : pairs) {
            auto pairIt = bpeRanks_.find(pair);
            if (pairIt != bpeRanks_.end()) {
                size_t rank = pairIt->second;
                if (rank < minRank) {
                    minRank = rank;
                    bigram = pair;
                }
            }
        }
        if (minRank >= bpeRanks_.size()) {
            break;
        }

        std::vector<std::wstring> newWord;
        size_t i = 0;
        while (i < word.size()) {
            bool foundFirst = false;
            size_t j = 0;
            for (j = i; j < word.size(); j++) {
                if (bigram.first == word[j]) {
                    foundFirst = true;
                    break;
                }
            }
            if (foundFirst) {
                for (size_t idx = i; idx < j; idx++) {
                    newWord.push_back(word[idx]);
                }
                i = j;
            } else {
                for (size_t idx = i; idx < word.size(); idx++) {
                    newWord.push_back(word[idx]);
                }
                break;
            }

            if (word[i] == bigram.first && i < word.size() - 1 && word[i + 1] == bigram.second) {
                newWord.push_back(bigram.first + bigram.second);
                i += 2;
            } else {
                newWord.push_back(word[i]);
                i += 1;
            }
        }

        word = newWord;
        if (word.size() == 1) {
            break;
        } else {
            pairs = getPairs(word);
        }
    }

    std::wstring wordStr = word[0];
    for (size_t idx = 1; idx < word.size(); idx++) {
        wordStr += L" " + word[idx];
    }
    cache_[token] = wordStr;
    return wordStr;
}



std::vector<int32_t> Encoder::encode(const std::string &text) {
    std::vector<int32_t> ret;
    std::string input = text;
    std::string token;
    std::wstring wToken;

    qDebug() << "Started encoding with text:" << QString::fromStdString(text);

    try {
        // Try matching the pattern to catch any potential regex errors
        boost::smatch match;
        int loopCount = 0;

        while (boost::regex_search(input, match, pattern)) {
            loopCount++;
            token = match[0];
            qDebug() << "Loop iteration:" << loopCount;
            qDebug() << "Matched token:" << QString::fromStdString(token);

            // Clear and populate wToken with encoded bytes
            wToken.clear();
            for (uint8_t b : token) {
                if (byteEncoder_.find(b) != byteEncoder_.end()) {
                    wToken.push_back(byteEncoder_[b]);
                } else {
                    qDebug() << "Warning: byte" << b << "not found in byteEncoder_";
                    wToken.push_back(L'?'); // Placeholder for undefined byte
                }
            }
            qDebug() << "wToken after byte encoding:" << QString::fromStdWString(wToken);

            // Apply BPE and split the result
            std::wstring bpeResult = bpe(wToken);
            auto bpeTokens = StringUtils::split(bpeResult, L' ');
            qDebug() << "BPE result:" << QString::fromStdWString(bpeResult);

            QStringList bpeTokensList;
            for (const auto& bt : bpeTokens) {
                bpeTokensList << QString::fromStdWString(bt);
            }
            qDebug() << "Split BPE tokens:" << bpeTokensList.join(" ");

            // Map BPE tokens to integer IDs in encoder_ and add to ret
            for (auto &bpeToken : bpeTokens) {
                if (encoder_.find(bpeToken) != encoder_.end()) {
                    int32_t tokenId = encoder_[bpeToken];
                    ret.push_back(tokenId);
                    qDebug() << "Token:" << QString::fromStdWString(bpeToken) << ", ID:" << tokenId;
                } else {
                    qDebug() << "Warning: BPE token" << QString::fromStdWString(bpeToken) << "not found in encoder_";
                }
            }

            // Consume the matched part of input and continue
            input = match.suffix().str();
        }
        qDebug() << "Total loop iterations:" << loopCount;
    } catch (const boost::regex_error& e) {
        // Catch and display any regex errors
        std::cerr << "Regex error: " << e.what() << std::endl;
        qDebug() << "Regex error:" << QString::fromStdString(e.what());
    }

    // Display final encoded token IDs
    QList<int> retList;
    for (const int32_t& id : ret) {
        retList.append(id);
    }
    qDebug() << "Final encoded token IDs:" << retList;

    return ret;
}



std::string Encoder::decode(const std::vector<int32_t> &tokens) {
    std::wstring text;
    for (int32_t idx : tokens) {
        text += decoder_[idx];
    }

    std::string ret;
    for (wchar_t c : text) {
        ret.push_back(char(byteDecoder_.at(c)));
    }

    return ret;
}


Encoder Encoder::getEncoder() {
    // Read "encoder.json" from the resource path
    QFile encoderFile(":/resources/encoder.json");
    if (!encoderFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // LOGE("open file failed: %s", ":/gpt2/tokenizer/resources/encoder.json");
        return {};
    }

    // Convert QTextStream content to std::string
    QTextStream encoderStream(&encoderFile);
    std::string encoderContent = encoderStream.readAll().toStdString();
    std::istringstream encoderIStream(encoderContent);

    // Now pass the std::istringstream to loadEncoderMap
    std::unordered_map<std::wstring, int32_t> encoderMap = loadEncoderMap(encoderIStream);

    // Read "vocab.bpe" from the resource path
    QFile vocabFile(":/resources/vocab.bpe");
    if (!vocabFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // LOGE("open file failed: %s", ":/gpt2/tokenizer/resources/vocab.bpe");
        return {};
    }

    // Convert QTextStream content to std::string for vocab.bpe
    QTextStream vocabStream(&vocabFile);
    std::string vocabContent = vocabStream.readAll().toStdString();
    std::istringstream vocabIStream(vocabContent);

    // Now pass the std::istringstream to loadVocabBpe
    std::vector<wstring_pair> bpeMerges = loadVocabBpe(vocabIStream);

    return {encoderMap, bpeMerges};
}



std::vector<wstring_pair> Encoder::getPairs(const std::vector<std::wstring> &word) {
    std::vector<wstring_pair> pairs;
    if (word.size() > 1) {
        auto previous = word[0];
        for (size_t i = 1; i < word.size(); i++) {
            pairs.emplace_back(previous, word[i]);
            previous = word[i];
        }
    }

    return pairs;
}

std::unordered_map<int32_t, wchar_t> Encoder::bytesToUnicode() {
    std::unordered_map<int32_t, wchar_t> b2u;

    auto setOriginByte = [&](int32_t start, int32_t end) {
        for (int32_t i = start; i <= end; i++) {
            b2u.insert({i, wchar_t(i)});
        }
    };

    setOriginByte(L'!', L'~');
    setOriginByte(L'¡', L'¬');
    setOriginByte(L'®', L'ÿ');

    int32_t n = 0;
    for (int32_t i = 0; i < 256; i++) {
        if (b2u.find(i) == b2u.end()) {
            b2u.insert({i, wchar_t(256 + n)});
            n++;
        }
    }

    return b2u;
}

std::unordered_map<std::wstring, int32_t> Encoder::loadEncoderMap(std::istream &in) {
    const auto json = FileUtils::parseJson(in);
    if (json.is_null()) {
        // LOGE("parse file failed: %s", GPT_ENCODER_JSON);
        return {};
    }

    std::unordered_map<std::wstring, int32_t> encoderMap;
    for (const auto &kv : json.items()) { // use items() for nlohmann::json
        encoderMap.insert({StringUtils::utf82wstring(kv.key()), kv.value().get<int32_t>()});
    }

    return encoderMap;
}

std::vector<wstring_pair> Encoder::loadVocabBpe(std::istream &in) {
    std::vector<wstring_pair> vocabBpe;
    std::string line;
    while (std::getline(in, line)) {
        // skip empty line or comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        size_t sep = line.find(' ');
        vocabBpe.emplace_back(
            StringUtils::utf82wstring(line.substr(0, sep)),
            StringUtils::utf82wstring(line.substr(sep + 1))
            );
    }

    return vocabBpe;
}
}

