#include <iostream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <stdexcept>
#include <chrono>
#include <sstream>
#include <unordered_set>
#include <numeric>
#include <limits>
#include <ctime>

extern "C"
{
#include "SpiceUsr.h"
}

namespace fs = std::filesystem;

// ============================================================
// CDP TESTING & DETECTION TOOLKIT
// VERSION 0.3 - 100 COVER BATCH RESEARCH RUNNER
// ============================================================
//
// WHAT THIS VERSION DOES
//
// 1. Recursively discovers every .txt file inside:
//
//      <CDP project folder>/dataset/synthetic-covers/
//
//    For backward compatibility, it also accepts:
//
//      <CDP project folder>/covers/
//
// 2. Requires at least 100 cover files.
//
// 3. Tests EVERY discovered cover the same number of times.
//
// 4. Every experiment uses:
//      - a newly generated test message
//      - a balanced random message length from 250-500 chars
//      - a unique message number
//      - a randomized UTC date/time
//      - a rotating celestial planet pair
//      - a freshly derived celestial seed
//
// 5. Every experiment:
//      - encodes the message
//      - decodes it with the correct seed
//      - repeats the encoding to verify determinism
//      - attempts decoding with a deliberately wrong seed
//      - measures character changes
//      - measures word changes
//      - records b/d/p/q distributions
//      - records capacity use
//      - records runtime
//
// 6. Every program run creates a NEW timestamped dataset:
//
//      toolkit_results/
//          session_YYYYMMDD_HHMMSS_<seed>/
//              experiments.csv
//              covers.csv
//              session_summary.txt
//              messages/
//              encoded/
//
// IMPORTANT:
//
// This is an EXPERIMENTAL RESEARCH TOOLKIT.
// The celestial seed method is not cryptographic encryption.
// ============================================================


// ============================================================
// DATA STRUCTURES
// ============================================================

struct TextStatistics
{
    std::size_t totalCharacters = 0;
    std::size_t wordCount = 0;
    std::size_t eligibleCharacters = 0;

    std::size_t bCount = 0;
    std::size_t dCount = 0;
    std::size_t pCount = 0;
    std::size_t qCount = 0;

    double bdBalance = 0.0;       // b / (b+d)
    double pqBalance = 0.0;       // p / (p+q)
    double eligibleEntropy = 0.0; // Shannon entropy across b,d,p,q
};

struct MutationStatistics
{
    std::size_t changedCharacters = 0;
    std::size_t changedWords = 0;

    std::size_t bToD = 0;
    std::size_t dToB = 0;
    std::size_t pToQ = 0;
    std::size_t qToP = 0;
};

struct PlanetPair
{
    std::string first;
    std::string second;
};

struct ExperimentResult
{
    std::size_t runNumber = 0;

    std::string coverFile;
    std::string category;

    std::size_t messageLength = 0;
    std::size_t payloadBits = 0;
    std::uint64_t messageNumber = 0;

    std::string utcTime;
    std::string planet1;
    std::string planet2;

    double distance1Km = 0.0;
    double distance2Km = 0.0;

    std::uint64_t celestialSeed = 0;
    std::uint64_t wrongSeed = 0;

    TextStatistics originalStats;
    TextStatistics encodedStats;
    MutationStatistics mutationStats;

    double capacityUsePercent = 0.0;
    double coverModificationPercent = 0.0;
    double eligibleModificationPercent = 0.0;

    bool decodePass = false;
    bool deterministicRepeatPass = false;
    bool wrongSeedRejected = false;

    double runtimeMilliseconds = 0.0;

    std::string messageFile;
    std::string encodedFile;
};


// ============================================================
// BASIC FILE HELPERS
// ============================================================

std::string readTextFile(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
    {
        throw std::runtime_error(
            "Could not open file: " + path.string()
        );
    }

    return std::string(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
}

void writeTextFile(
    const fs::path& path,
    const std::string& text
)
{
    std::ofstream file(path, std::ios::binary);

    if (!file)
    {
        throw std::runtime_error(
            "Could not create file: " + path.string()
        );
    }

    file.write(
        text.data(),
        static_cast<std::streamsize>(text.size())
    );
}

std::string removeQuotes(std::string text)
{
    if (
        text.size() >= 2 &&
        text.front() == '"' &&
        text.back() == '"'
    )
    {
        text =
            text.substr(
                1,
                text.size() - 2
            );
    }

    return text;
}

std::string toLowerCopy(std::string text)
{
    std::transform(
        text.begin(),
        text.end(),
        text.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        }
    );

    return text;
}

std::string csvEscape(const std::string& value)
{
    bool needsQuotes =
        value.find(',') != std::string::npos ||
        value.find('"') != std::string::npos ||
        value.find('\n') != std::string::npos ||
        value.find('\r') != std::string::npos;

    if (!needsQuotes)
    {
        return value;
    }

    std::string result = "\"";

    for (char c : value)
    {
        if (c == '"')
        {
            result += "\"\"";
        }
        else
        {
            result += c;
        }
    }

    result += "\"";

    return result;
}


// ============================================================
// SESSION / TIME HELPERS
// ============================================================

std::tm localTimeSafe(std::time_t value)
{
    std::tm result{};

#ifdef _WIN32
    localtime_s(&result, &value);
#else
    localtime_r(&value, &result);
#endif

    return result;
}

std::string makeSessionTimestamp()
{
    std::time_t now =
        std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now()
        );

    std::tm local =
        localTimeSafe(now);

    std::ostringstream output;

    output
        << std::put_time(
            &local,
            "%Y%m%d_%H%M%S"
        );

    return output.str();
}


// ============================================================
// TEXT STATISTICS
// ============================================================

double shannonEntropy4(
    std::size_t a,
    std::size_t b,
    std::size_t c,
    std::size_t d
)
{
    const double total =
        static_cast<double>(a + b + c + d);

    if (total <= 0.0)
    {
        return 0.0;
    }

    double entropy = 0.0;

    const std::size_t values[4] =
    {
        a, b, c, d
    };

    for (std::size_t value : values)
    {
        if (value == 0)
        {
            continue;
        }

        const double probability =
            static_cast<double>(value) / total;

        entropy -=
            probability *
            std::log2(probability);
    }

    return entropy;
}

TextStatistics analyzeText(
    const std::string& text
)
{
    TextStatistics stats;

    stats.totalCharacters =
        text.size();

    bool insideWord = false;

    for (char character : text)
    {
        const unsigned char raw =
            static_cast<unsigned char>(
                character
            );

        const bool wordCharacter =
            std::isalnum(raw) != 0 ||
            character == '\'';

        if (wordCharacter && !insideWord)
        {
            ++stats.wordCount;
            insideWord = true;
        }
        else if (!wordCharacter)
        {
            insideWord = false;
        }

        const char lower =
            static_cast<char>(
                std::tolower(raw)
            );

        switch (lower)
        {
        case 'b':
            ++stats.bCount;
            ++stats.eligibleCharacters;
            break;

        case 'd':
            ++stats.dCount;
            ++stats.eligibleCharacters;
            break;

        case 'p':
            ++stats.pCount;
            ++stats.eligibleCharacters;
            break;

        case 'q':
            ++stats.qCount;
            ++stats.eligibleCharacters;
            break;

        default:
            break;
        }
    }

    const double bdTotal =
        static_cast<double>(
            stats.bCount + stats.dCount
        );

    if (bdTotal > 0.0)
    {
        stats.bdBalance =
            static_cast<double>(
                stats.bCount
            ) / bdTotal;
    }

    const double pqTotal =
        static_cast<double>(
            stats.pCount + stats.qCount
        );

    if (pqTotal > 0.0)
    {
        stats.pqBalance =
            static_cast<double>(
                stats.pCount
            ) / pqTotal;
    }

    stats.eligibleEntropy =
        shannonEntropy4(
            stats.bCount,
            stats.dCount,
            stats.pCount,
            stats.qCount
        );

    return stats;
}

double calculatePercent(
    std::size_t amount,
    std::size_t total
)
{
    if (total == 0)
    {
        return 0.0;
    }

    return
        (
            static_cast<double>(amount)
            /
            static_cast<double>(total)
        )
        * 100.0;
}


// ============================================================
// CDP ELIGIBLE CHARACTER FUNCTIONS
// ============================================================

bool isEligibleCharacter(char character)
{
    char lower =
        static_cast<char>(
            std::tolower(
                static_cast<unsigned char>(
                    character
                )
            )
        );

    return
        lower == 'b' ||
        lower == 'd' ||
        lower == 'p' ||
        lower == 'q';
}

std::vector<std::size_t> findEligiblePositions(
    const std::string& text
)
{
    std::vector<std::size_t> positions;

    positions.reserve(
        text.size() / 5
    );

    for (
        std::size_t i = 0;
        i < text.size();
        ++i
    )
    {
        if (isEligibleCharacter(text[i]))
        {
            positions.push_back(i);
        }
    }

    return positions;
}


// ============================================================
// MESSAGE -> BITS
// ============================================================

std::vector<int> messageToBits(
    const std::string& message
)
{
    if (message.size() > UINT32_MAX)
    {
        throw std::runtime_error(
            "Message is too large."
        );
    }

    std::vector<int> bits;

    bits.reserve(
        32 + message.size() * 8
    );

    std::uint32_t messageLength =
        static_cast<std::uint32_t>(
            message.size()
        );

    for (int bit = 31; bit >= 0; --bit)
    {
        bits.push_back(
            (messageLength >> bit) & 1
        );
    }

    for (unsigned char character : message)
    {
        for (int bit = 7; bit >= 0; --bit)
        {
            bits.push_back(
                (character >> bit) & 1
            );
        }
    }

    return bits;
}


// ============================================================
// BIT ENCODING / DECODING
// ============================================================

char writeBit(
    char original,
    int bit
)
{
    const bool uppercase =
        std::isupper(
            static_cast<unsigned char>(
                original
            )
        ) != 0;

    const char lower =
        static_cast<char>(
            std::tolower(
                static_cast<unsigned char>(
                    original
                )
            )
        );

    char result = original;

    if (lower == 'b' || lower == 'd')
    {
        result =
            bit == 0
            ? 'b'
            : 'd';
    }
    else if (
        lower == 'p' ||
        lower == 'q'
    )
    {
        result =
            bit == 0
            ? 'p'
            : 'q';
    }

    if (uppercase)
    {
        result =
            static_cast<char>(
                std::toupper(
                    static_cast<unsigned char>(
                        result
                    )
                )
            );
    }

    return result;
}

int readBit(char character)
{
    const char lower =
        static_cast<char>(
            std::tolower(
                static_cast<unsigned char>(
                    character
                )
            )
        );

    if (
        lower == 'b' ||
        lower == 'p'
    )
    {
        return 0;
    }

    if (
        lower == 'd' ||
        lower == 'q'
    )
    {
        return 1;
    }

    return -1;
}

std::string encodeMessage(
    const std::string& coverText,
    const std::string& message,
    std::uint64_t seed
)
{
    std::vector<int> bits =
        messageToBits(message);

    std::vector<std::size_t> positions =
        findEligiblePositions(
            coverText
        );

    if (bits.size() > positions.size())
    {
        throw std::runtime_error(
            "Cover does not have enough "
            "eligible b/d/p/q positions."
        );
    }

    std::mt19937_64 randomGenerator(seed);

    std::shuffle(
        positions.begin(),
        positions.end(),
        randomGenerator
    );

    std::string encodedText =
        coverText;

    for (
        std::size_t i = 0;
        i < bits.size();
        ++i
    )
    {
        const std::size_t selected =
            positions[i];

        encodedText[selected] =
            writeBit(
                encodedText[selected],
                bits[i]
            );
    }

    return encodedText;
}

std::string decodeMessage(
    const std::string& encodedText,
    std::uint64_t seed
)
{
    std::vector<std::size_t> positions =
        findEligiblePositions(
            encodedText
        );

    if (positions.size() < 32)
    {
        throw std::runtime_error(
            "Not enough eligible positions "
            "for a message header."
        );
    }

    std::mt19937_64 randomGenerator(seed);

    std::shuffle(
        positions.begin(),
        positions.end(),
        randomGenerator
    );

    std::uint32_t messageLength = 0;

    for (int i = 0; i < 32; ++i)
    {
        const int bit =
            readBit(
                encodedText[
                    positions[
                        static_cast<std::size_t>(i)
                    ]
                ]
            );

        if (bit < 0)
        {
            throw std::runtime_error(
                "Invalid bit while decoding header."
            );
        }

        messageLength =
            (messageLength << 1) |
            static_cast<std::uint32_t>(bit);
    }

    const std::size_t requiredBits =
        32 +
        static_cast<std::size_t>(
            messageLength
        ) * 8;

    if (requiredBits > positions.size())
    {
        throw std::runtime_error(
            "Decoded message length exceeds "
            "available capacity."
        );
    }

    // Extra safety against absurd wrong-seed header values.
    if (messageLength > 100000)
    {
        throw std::runtime_error(
            "Decoded message length is unreasonable."
        );
    }

    std::string recovered;

    recovered.reserve(
        messageLength
    );

    std::size_t positionIndex = 32;

    for (
        std::uint32_t characterIndex = 0;
        characterIndex < messageLength;
        ++characterIndex
    )
    {
        unsigned char character = 0;

        for (int bitIndex = 0; bitIndex < 8; ++bitIndex)
        {
            const int bit =
                readBit(
                    encodedText[
                        positions[
                            positionIndex
                        ]
                    ]
                );

            if (bit < 0)
            {
                throw std::runtime_error(
                    "Invalid bit while decoding payload."
                );
            }

            character =
                static_cast<unsigned char>(
                    (character << 1) |
                    bit
                );

            ++positionIndex;
        }

        recovered.push_back(
            static_cast<char>(
                character
            )
        );
    }

    return recovered;
}


// ============================================================
// MUTATION ANALYSIS
// ============================================================

MutationStatistics analyzeMutations(
    const std::string& original,
    const std::string& encoded
)
{
    if (original.size() != encoded.size())
    {
        throw std::runtime_error(
            "Original and encoded cover lengths differ."
        );
    }

    MutationStatistics stats;

    bool insideWord = false;
    bool currentWordChanged = false;

    for (
        std::size_t i = 0;
        i < original.size();
        ++i
    )
    {
        const char before =
            original[i];

        const char after =
            encoded[i];

        const unsigned char raw =
            static_cast<unsigned char>(
                before
            );

        const bool wordCharacter =
            std::isalnum(raw) != 0 ||
            before == '\'';

        if (wordCharacter)
        {
            if (!insideWord)
            {
                insideWord = true;
                currentWordChanged = false;
            }

            if (before != after)
            {
                currentWordChanged = true;
            }
        }
        else
        {
            if (
                insideWord &&
                currentWordChanged
            )
            {
                ++stats.changedWords;
            }

            insideWord = false;
            currentWordChanged = false;
        }

        if (before == after)
        {
            continue;
        }

        ++stats.changedCharacters;

        const char lowerBefore =
            static_cast<char>(
                std::tolower(
                    static_cast<unsigned char>(
                        before
                    )
                )
            );

        const char lowerAfter =
            static_cast<char>(
                std::tolower(
                    static_cast<unsigned char>(
                        after
                    )
                )
            );

        if (
            lowerBefore == 'b' &&
            lowerAfter == 'd'
        )
        {
            ++stats.bToD;
        }
        else if (
            lowerBefore == 'd' &&
            lowerAfter == 'b'
        )
        {
            ++stats.dToB;
        }
        else if (
            lowerBefore == 'p' &&
            lowerAfter == 'q'
        )
        {
            ++stats.pToQ;
        }
        else if (
            lowerBefore == 'q' &&
            lowerAfter == 'p'
        )
        {
            ++stats.qToP;
        }
    }

    if (
        insideWord &&
        currentWordChanged
    )
    {
        ++stats.changedWords;
    }

    return stats;
}


// ============================================================
// CELESTIAL FUNCTIONS
// ============================================================

SpiceDouble getPlanetDistance(
    const char* target,
    SpiceDouble et
)
{
    SpiceDouble position[3];
    SpiceDouble lightTime;

    spkpos_c(
        target,
        et,
        "J2000",
        "NONE",
        "EARTH",
        position,
        &lightTime
    );

    return vnorm_c(position);
}

std::uint64_t reduceToSeedRange(
    long double value
)
{
    const long double MODULUS =
        1000000007.0L;

    const long double reduced =
        std::fmod(
            std::fabs(value),
            MODULUS
        );

    return
        static_cast<std::uint64_t>(
            reduced
        );
}

std::uint64_t deriveCelestialSeed(
    SpiceDouble distance1,
    SpiceDouble distance2,
    std::uint64_t messageNum
)
{
    const long double ratio =
        static_cast<long double>(
            distance1
        )
        /
        static_cast<long double>(
            distance2
        );

    const long double scaledRatio =
        ratio * 1000000.0L;

    const long double combined =
        static_cast<long double>(
            distance1
        )
        *
        scaledRatio;

    const std::uint64_t celestialValue =
        reduceToSeedRange(
            combined
        );

    return
        celestialValue ^
        messageNum;
}


// ============================================================
// RANDOM MESSAGE GENERATOR
// ============================================================
//
// Messages are intentionally ordinary English-style text.
// They are not used as cover text.
// Their purpose is to provide different payload bit patterns.
// ============================================================

std::string randomChoice(
    const std::vector<std::string>& values,
    std::mt19937_64& rng
)
{
    std::uniform_int_distribution<std::size_t>
        distribution(
            0,
            values.size() - 1
        );

    return
        values[
            distribution(rng)
        ];
}

std::string generateSentence(
    std::mt19937_64& rng
)
{
    static const std::vector<std::string> subjects =
    {
        "The project team",
        "A small research group",
        "The operations manager",
        "Several students",
        "The technical staff",
        "A regional office",
        "The review committee",
        "The development team",
        "A customer support group",
        "The planning department"
    };

    static const std::vector<std::string> verbs =
    {
        "reviewed",
        "compared",
        "documented",
        "tested",
        "measured",
        "discussed",
        "evaluated",
        "organized",
        "verified",
        "prepared"
    };

    static const std::vector<std::string> objects =
    {
        "the latest results",
        "several possible approaches",
        "a set of recorded observations",
        "the current project data",
        "a revised process",
        "the available evidence",
        "a new technical procedure",
        "the previous test results",
        "a collection of sample records",
        "the planned changes"
    };

    static const std::vector<std::string> endings =
    {
        "before making a final decision",
        "and recorded the outcome for later review",
        "so the result could be reproduced",
        "while keeping the main goal in mind",
        "and identified several questions for follow-up",
        "before beginning the next stage",
        "to determine whether the pattern was consistent",
        "and compared the result with the original baseline",
        "without assuming that one example proved the larger claim",
        "so future tests could use the same procedure"
    };

    std::ostringstream sentence;

    sentence
        << randomChoice(
            subjects,
            rng
        )
        << " "
        << randomChoice(
            verbs,
            rng
        )
        << " "
        << randomChoice(
            objects,
            rng
        )
        << " "
        << randomChoice(
            endings,
            rng
        )
        << ".";

    return sentence.str();
}

std::string generateUniqueTestMessage(
    std::size_t targetLength,
    std::mt19937_64& rng,
    std::uint64_t runNumber
)
{
    std::ostringstream builder;

    builder
        << "Experiment "
        << runNumber
        << " begins with a newly generated test message. ";

    while (
        builder.str().size() <
        targetLength + 120
    )
    {
        builder
            << generateSentence(rng)
            << " ";
    }

    std::string message =
        builder.str();

    // Keep the message inside the requested size without cutting
    // immediately after the minimum boundary.
    if (message.size() > targetLength)
    {
        std::size_t cut =
            message.rfind(
                ' ',
                targetLength - 1
            );

        if (
            cut == std::string::npos ||
            cut < 240
        )
        {
            cut =
                targetLength - 1;
        }

        message.resize(cut);

        while (
            !message.empty() &&
            std::isspace(
                static_cast<unsigned char>(
                    message.back()
                )
            )
        )
        {
            message.pop_back();
        }

        message += ".";
    }

    // Ensure the experiment stays inside the original 250-500 target.
    while (message.size() < 250)
    {
        message +=
            " The test records another observation.";
    }

    if (message.size() > 500)
    {
        message.resize(499);
        message += ".";
    }

    return message;
}


// ============================================================
// RANDOM DATE GENERATION
// ============================================================

std::string generateRandomUtcTime(
    std::mt19937_64& rng,
    std::unordered_set<std::string>& usedTimes
)
{
    static const char* months[12] =
    {
        "JAN", "FEB", "MAR", "APR",
        "MAY", "JUN", "JUL", "AUG",
        "SEP", "OCT", "NOV", "DEC"
    };

    std::uniform_int_distribution<int>
        yearDistribution(
            2020,
            2035
        );

    std::uniform_int_distribution<int>
        monthDistribution(
            0,
            11
        );

    // Day 1-28 guarantees a valid date in every month.
    std::uniform_int_distribution<int>
        dayDistribution(
            1,
            28
        );

    std::uniform_int_distribution<int>
        hourDistribution(
            0,
            23
        );

    std::uniform_int_distribution<int>
        minuteSecondDistribution(
            0,
            59
        );

    for (;;)
    {
        std::ostringstream value;

        value
            << yearDistribution(rng)
            << " "
            << months[
                monthDistribution(rng)
            ]
            << " "
            << std::setw(2)
            << std::setfill('0')
            << dayDistribution(rng)
            << " "
            << std::setw(2)
            << hourDistribution(rng)
            << ":"
            << std::setw(2)
            << minuteSecondDistribution(rng)
            << ":"
            << std::setw(2)
            << minuteSecondDistribution(rng)
            << " UTC";

        const std::string result =
            value.str();

        if (
            usedTimes.insert(
                result
            ).second
        )
        {
            return result;
        }
    }
}


// ============================================================
// COVER DISCOVERY
// ============================================================

std::vector<fs::path> discoverCoverFiles(
    const fs::path& coversFolder
)
{
    if (!fs::exists(coversFolder))
    {
        throw std::runtime_error(
            "The covers folder does not exist: " +
            coversFolder.string()
        );
    }

    std::vector<fs::path> files;

    for (
        const auto& entry :
        fs::recursive_directory_iterator(
            coversFolder
        )
    )
    {
        if (
            !entry.is_regular_file()
        )
        {
            continue;
        }

        const fs::path path =
            entry.path();

        if (
            toLowerCopy(
                path.extension().string()
            )
            == ".txt"
        )
        {
            files.push_back(path);
        }
    }

    std::sort(
        files.begin(),
        files.end()
    );

    return files;
}

std::string inferCategory(
    const fs::path& path
)
{
    const std::string name =
        toLowerCopy(
            path.filename().string()
        );

    if (
        name.find("business") !=
        std::string::npos
    )
    {
        return "business";
    }

    if (
        name.find("technical") !=
        std::string::npos
    )
    {
        return "technical";
    }

    if (
        name.find("narrative") !=
            std::string::npos ||
        name.find("story") !=
            std::string::npos
    )
    {
        return "narrative";
    }

    if (
        name.find("news") !=
        std::string::npos
    )
    {
        return "news";
    }

    if (
        name.find("casual") !=
        std::string::npos
    )
    {
        return "casual";
    }

    if (
        name.find("academic") !=
        std::string::npos
    )
    {
        return "academic";
    }

    if (
        name.find("instruction") !=
        std::string::npos
    )
    {
        return "instructions";
    }

    if (
        name.find("report") !=
        std::string::npos
    )
    {
        return "reports";
    }

    if (
        name.find("review") !=
        std::string::npos
    )
    {
        return "reviews";
    }

    if (
        name.find("general") !=
        std::string::npos
    )
    {
        return "general";
    }

    return "uncategorized";
}


// ============================================================
// PLANET PAIR ROTATION
// ============================================================

std::vector<PlanetPair> createAllPlanetPairs()
{
    const std::vector<std::string> planets =
    {
        "MERCURY BARYCENTER",
        "VENUS BARYCENTER",
        "MARS BARYCENTER",
        "JUPITER BARYCENTER",
        "SATURN BARYCENTER",
        "URANUS BARYCENTER",
        "NEPTUNE BARYCENTER"
    };

    std::vector<PlanetPair> pairs;

    for (
        const std::string& first :
        planets
    )
    {
        for (
            const std::string& second :
            planets
        )
        {
            if (first == second)
            {
                continue;
            }

            pairs.push_back(
                {
                    first,
                    second
                }
            );
        }
    }

    return pairs;
}

class PlanetPairRotator
{
public:

    explicit PlanetPairRotator(
        std::mt19937_64& generator
    )
        :
        rng(generator),
        pairs(
            createAllPlanetPairs()
        )
    {
        reshuffle();
    }

    PlanetPair next()
    {
        if (index >= pairs.size())
        {
            reshuffle();
        }

        PlanetPair result =
            pairs[index++];

        last =
            result;

        haveLast =
            true;

        return result;
    }

private:

    std::mt19937_64& rng;
    std::vector<PlanetPair> pairs;
    std::size_t index = 0;

    PlanetPair last;
    bool haveLast = false;

    void reshuffle()
    {
        std::shuffle(
            pairs.begin(),
            pairs.end(),
            rng
        );

        // Avoid repeating the same pair at the boundary
        // between two complete 42-pair cycles.
        if (
            haveLast &&
            !pairs.empty() &&
            pairs.front().first ==
                last.first &&
            pairs.front().second ==
                last.second
        )
        {
            if (pairs.size() > 1)
            {
                std::swap(
                    pairs[0],
                    pairs[1]
                );
            }
        }

        index = 0;
    }
};


// ============================================================
// WRONG-SEED TEST
// ============================================================

bool wrongSeedFailsToRecover(
    const std::string& encodedText,
    const std::string& originalMessage,
    std::uint64_t wrongSeed
)
{
    try
    {
        const std::string recovered =
            decodeMessage(
                encodedText,
                wrongSeed
            );

        return
            recovered !=
            originalMessage;
    }
    catch (...)
    {
        // A wrong seed producing an invalid header / payload
        // counts as successfully rejecting the wrong seed.
        return true;
    }
}


// ============================================================
// COVERS CSV
// ============================================================

void writeCoversCsv(
    const fs::path& path,
    const std::vector<fs::path>& covers
)
{
    std::ofstream file(path);

    if (!file)
    {
        throw std::runtime_error(
            "Could not create covers.csv"
        );
    }

    file
        << "cover_file,"
        << "category,"
        << "total_characters,"
        << "word_count,"
        << "eligible_characters,"
        << "b_count,"
        << "d_count,"
        << "p_count,"
        << "q_count,"
        << "bd_balance,"
        << "pq_balance,"
        << "eligible_entropy,"
        << "max_message_chars_theoretical\n";

    file
        << std::fixed
        << std::setprecision(8);

    for (
        const fs::path& coverPath :
        covers
    )
    {
        const std::string text =
            readTextFile(
                coverPath
            );

        const TextStatistics stats =
            analyzeText(
                text
            );

        std::size_t maximumMessageCharacters = 0;

        if (stats.eligibleCharacters >= 32)
        {
            maximumMessageCharacters =
                (
                    stats.eligibleCharacters -
                    32
                ) / 8;
        }

        file
            << csvEscape(
                coverPath.filename().string()
            )
            << ","
            << csvEscape(
                inferCategory(
                    coverPath
                )
            )
            << ","
            << stats.totalCharacters
            << ","
            << stats.wordCount
            << ","
            << stats.eligibleCharacters
            << ","
            << stats.bCount
            << ","
            << stats.dCount
            << ","
            << stats.pCount
            << ","
            << stats.qCount
            << ","
            << stats.bdBalance
            << ","
            << stats.pqBalance
            << ","
            << stats.eligibleEntropy
            << ","
            << maximumMessageCharacters
            << "\n";
    }
}


// ============================================================
// EXPERIMENT CSV
// ============================================================

void writeExperimentCsvHeader(
    std::ofstream& file
)
{
    file
        << "run_number,"
        << "cover_file,"
        << "category,"
        << "message_length,"
        << "payload_bits,"
        << "message_number,"
        << "utc_time,"
        << "planet_1,"
        << "planet_2,"
        << "distance_1_km,"
        << "distance_2_km,"
        << "celestial_seed,"
        << "wrong_seed,"
        << "cover_characters,"
        << "cover_words,"
        << "eligible_positions,"
        << "capacity_use_percent,"
        << "changed_characters,"
        << "cover_modification_percent,"
        << "eligible_modification_percent,"
        << "changed_words,"
        << "b_to_d,"
        << "d_to_b,"
        << "p_to_q,"
        << "q_to_p,"
        << "original_b,"
        << "original_d,"
        << "original_p,"
        << "original_q,"
        << "encoded_b,"
        << "encoded_d,"
        << "encoded_p,"
        << "encoded_q,"
        << "original_bd_balance,"
        << "encoded_bd_balance,"
        << "original_pq_balance,"
        << "encoded_pq_balance,"
        << "original_eligible_entropy,"
        << "encoded_eligible_entropy,"
        << "decode_pass,"
        << "deterministic_repeat_pass,"
        << "wrong_seed_rejected,"
        << "runtime_ms,"
        << "message_file,"
        << "encoded_file\n";
}

void writeExperimentCsvRow(
    std::ofstream& file,
    const ExperimentResult& r
)
{
    file
        << r.runNumber
        << ","
        << csvEscape(r.coverFile)
        << ","
        << csvEscape(r.category)
        << ","
        << r.messageLength
        << ","
        << r.payloadBits
        << ","
        << r.messageNumber
        << ","
        << csvEscape(r.utcTime)
        << ","
        << csvEscape(r.planet1)
        << ","
        << csvEscape(r.planet2)
        << ","
        << r.distance1Km
        << ","
        << r.distance2Km
        << ","
        << r.celestialSeed
        << ","
        << r.wrongSeed
        << ","
        << r.originalStats.totalCharacters
        << ","
        << r.originalStats.wordCount
        << ","
        << r.originalStats.eligibleCharacters
        << ","
        << r.capacityUsePercent
        << ","
        << r.mutationStats.changedCharacters
        << ","
        << r.coverModificationPercent
        << ","
        << r.eligibleModificationPercent
        << ","
        << r.mutationStats.changedWords
        << ","
        << r.mutationStats.bToD
        << ","
        << r.mutationStats.dToB
        << ","
        << r.mutationStats.pToQ
        << ","
        << r.mutationStats.qToP
        << ","
        << r.originalStats.bCount
        << ","
        << r.originalStats.dCount
        << ","
        << r.originalStats.pCount
        << ","
        << r.originalStats.qCount
        << ","
        << r.encodedStats.bCount
        << ","
        << r.encodedStats.dCount
        << ","
        << r.encodedStats.pCount
        << ","
        << r.encodedStats.qCount
        << ","
        << r.originalStats.bdBalance
        << ","
        << r.encodedStats.bdBalance
        << ","
        << r.originalStats.pqBalance
        << ","
        << r.encodedStats.pqBalance
        << ","
        << r.originalStats.eligibleEntropy
        << ","
        << r.encodedStats.eligibleEntropy
        << ","
        << (r.decodePass ? "PASS" : "FAIL")
        << ","
        << (
            r.deterministicRepeatPass
            ? "PASS"
            : "FAIL"
        )
        << ","
        << (
            r.wrongSeedRejected
            ? "PASS"
            : "FAIL"
        )
        << ","
        << r.runtimeMilliseconds
        << ","
        << csvEscape(r.messageFile)
        << ","
        << csvEscape(r.encodedFile)
        << "\n";
}


// ============================================================
// SUMMARY STATISTICS
// ============================================================

double mean(
    const std::vector<double>& values
)
{
    if (values.empty())
    {
        return 0.0;
    }

    return
        std::accumulate(
            values.begin(),
            values.end(),
            0.0
        )
        /
        static_cast<double>(
            values.size()
        );
}

double median(
    std::vector<double> values
)
{
    if (values.empty())
    {
        return 0.0;
    }

    std::sort(
        values.begin(),
        values.end()
    );

    const std::size_t middle =
        values.size() / 2;

    if (values.size() % 2 == 1)
    {
        return
            values[middle];
    }

    return
        (
            values[middle - 1] +
            values[middle]
        )
        / 2.0;
}

void writeSummary(
    const fs::path& path,
    std::uint64_t masterSeed,
    std::size_t coverCount,
    std::size_t runsPerCover,
    const std::vector<ExperimentResult>& results
)
{
    std::ofstream file(path);

    if (!file)
    {
        throw std::runtime_error(
            "Could not create session_summary.txt"
        );
    }

    std::size_t decodePasses = 0;
    std::size_t deterministicPasses = 0;
    std::size_t wrongSeedPasses = 0;

    std::vector<double>
        modificationRates;

    std::vector<double>
        eligibleModificationRates;

    std::vector<double>
        capacityRates;

    std::vector<double>
        runtimes;

    std::vector<double>
        changedWords;

    for (
        const ExperimentResult& r :
        results
    )
    {
        if (r.decodePass)
        {
            ++decodePasses;
        }

        if (r.deterministicRepeatPass)
        {
            ++deterministicPasses;
        }

        if (r.wrongSeedRejected)
        {
            ++wrongSeedPasses;
        }

        modificationRates.push_back(
            r.coverModificationPercent
        );

        eligibleModificationRates.push_back(
            r.eligibleModificationPercent
        );

        capacityRates.push_back(
            r.capacityUsePercent
        );

        runtimes.push_back(
            r.runtimeMilliseconds
        );

        changedWords.push_back(
            static_cast<double>(
                r.mutationStats.changedWords
            )
        );
    }

    file
        << "CDP TESTING & DETECTION TOOLKIT\n"
        << "SESSION SUMMARY\n"
        << "==========================================\n\n";

    file
        << "Master random seed: "
        << masterSeed
        << "\n";

    file
        << "Cover files tested: "
        << coverCount
        << "\n";

    file
        << "Runs per cover: "
        << runsPerCover
        << "\n";

    file
        << "Total experiments: "
        << results.size()
        << "\n\n";

    file
        << "Correct-seed decode passes: "
        << decodePasses
        << " / "
        << results.size()
        << "\n";

    file
        << "Deterministic repeat passes: "
        << deterministicPasses
        << " / "
        << results.size()
        << "\n";

    file
        << "Wrong-seed rejection passes: "
        << wrongSeedPasses
        << " / "
        << results.size()
        << "\n\n";

    file
        << std::fixed
        << std::setprecision(6);

    file
        << "Mean cover modification %: "
        << mean(
            modificationRates
        )
        << "\n";

    file
        << "Median cover modification %: "
        << median(
            modificationRates
        )
        << "\n";

    file
        << "Mean eligible-position modification %: "
        << mean(
            eligibleModificationRates
        )
        << "\n";

    file
        << "Mean capacity use %: "
        << mean(
            capacityRates
        )
        << "\n";

    file
        << "Mean changed words: "
        << mean(
            changedWords
        )
        << "\n";

    file
        << "Mean runtime per experiment (ms): "
        << mean(
            runtimes
        )
        << "\n";

    file
        << "\nNOTES\n"
        << "------------------------------------------\n"
        << "The celestial seed method tested here is experimental.\n"
        << "Public planetary data should not be treated as cryptographic secrecy.\n"
        << "A PASS means the implementation behaved as expected in this test.\n"
        << "It does not establish security or undetectability.\n";
}


// ============================================================
// MAIN PROGRAM
// ============================================================

int main()
{
    try
    {
        std::cout
            << "CDP TESTING & DETECTION TOOLKIT\n"
            << "VERSION 0.3 - 100 COVER BATCH RUNNER\n"
            << "==========================================\n\n";

        // ----------------------------------------------------
        // ASK FOR THE ORIGINAL CDP PROJECT FOLDER
        // ----------------------------------------------------

        std::cout
            << "Enter your ORIGINAL CDP project folder.\n"
            << "\n"
            << "It must contain:\n"
            << "  dataset/synthetic-covers/\n"
            << "  kernels/naif0012.tls\n"
            << "  kernels/de440.bsp\n"
            << "\n"
            << "Paste folder path here:\n> ";

        std::string projectInput;

        std::getline(
            std::cin,
            projectInput
        );

        projectInput =
            removeQuotes(
                projectInput
            );

        if (projectInput.empty())
        {
            throw std::runtime_error(
                "No project folder was entered."
            );
        }

        const fs::path projectFolder =
            fs::path(
                projectInput
            );

        // Preferred GitHub repository layout.
        fs::path coversFolder =
            projectFolder /
            "dataset" /
            "synthetic-covers";

        // Backward compatibility for the original local test layout.
        if (!fs::exists(coversFolder))
        {
            const fs::path legacyCoversFolder =
                projectFolder /
                "covers";

            if (fs::exists(legacyCoversFolder))
            {
                coversFolder =
                    legacyCoversFolder;
            }
        }

        const fs::path leapsecondsKernel =
            projectFolder /
            "kernels" /
            "naif0012.tls";

        const fs::path ephemerisKernel =
            projectFolder /
            "kernels" /
            "de440.bsp";

        if (!fs::exists(leapsecondsKernel))
        {
            throw std::runtime_error(
                "Missing kernel: " +
                leapsecondsKernel.string()
            );
        }

        if (!fs::exists(ephemerisKernel))
        {
            throw std::runtime_error(
                "Missing kernel: " +
                ephemerisKernel.string()
            );
        }

        // ----------------------------------------------------
        // DISCOVER ALL COVER FILES
        // ----------------------------------------------------

        std::vector<fs::path> covers =
            discoverCoverFiles(
                coversFolder
            );

        std::cout
            << "\nCover files discovered: "
            << covers.size()
            << "\n";

        if (covers.size() < 100)
        {
            throw std::runtime_error(
                "At least 100 .txt cover files are required "
                "inside the covers folder."
            );
        }

        if (covers.size() != 100)
        {
            std::cout
                << "NOTE: This toolkit tests every .txt file found.\n"
                << "You currently have "
                << covers.size()
                << " covers, so all "
                << covers.size()
                << " will be tested.\n";
        }

        // ----------------------------------------------------
        // RUNS PER COVER
        // ----------------------------------------------------

        std::cout
            << "\nRuns per cover [default 5, range 1-20]:\n> ";

        std::string runsInput;

        std::getline(
            std::cin,
            runsInput
        );

        std::size_t runsPerCover = 5;

        if (!runsInput.empty())
        {
            runsPerCover =
                static_cast<std::size_t>(
                    std::stoul(
                        runsInput
                    )
                );
        }

        if (
            runsPerCover < 1 ||
            runsPerCover > 20
        )
        {
            throw std::runtime_error(
                "Runs per cover must be between 1 and 20."
            );
        }

        const std::size_t totalExperiments =
            covers.size() *
            runsPerCover;

        std::cout
            << "\nTotal experiments this session: "
            << totalExperiments
            << "\n";

        // ----------------------------------------------------
        // MASTER RANDOM SEED
        // ----------------------------------------------------

        std::cout
            << "\nMaster random seed [0 = create a new random session]:\n> ";

        std::string seedInput;

        std::getline(
            std::cin,
            seedInput
        );

        std::uint64_t masterSeed = 0;

        if (
            !seedInput.empty() &&
            seedInput != "0"
        )
        {
            masterSeed =
                static_cast<std::uint64_t>(
                    std::stoull(
                        seedInput
                    )
                );
        }
        else
        {
            std::random_device device;

            const std::uint64_t timePart =
                static_cast<std::uint64_t>(
                    std::chrono::high_resolution_clock::
                    now().
                    time_since_epoch().
                    count()
                );

            const std::uint64_t randomPart =
                (
                    static_cast<std::uint64_t>(
                        device()
                    ) << 32
                )
                ^
                static_cast<std::uint64_t>(
                    device()
                );

            masterSeed =
                timePart ^
                randomPart;
        }

        std::mt19937_64 rng(
            masterSeed
        );

        std::shuffle(
            covers.begin(),
            covers.end(),
            rng
        );

        std::cout
            << "\nMaster random seed: "
            << masterSeed
            << "\n";

        // ----------------------------------------------------
        // CREATE UNIQUE SESSION OUTPUT FOLDER
        // ----------------------------------------------------

        const fs::path resultsRoot =
            projectFolder /
            "toolkit_results";

        fs::create_directories(
            resultsRoot
        );

        std::ostringstream folderName;

        folderName
            << "session_"
            << makeSessionTimestamp()
            << "_"
            << masterSeed;

        const fs::path sessionFolder =
            resultsRoot /
            folderName.str();

        const fs::path messagesFolder =
            sessionFolder /
            "messages";

        const fs::path encodedFolder =
            sessionFolder /
            "encoded";

        fs::create_directories(
            messagesFolder
        );

        fs::create_directories(
            encodedFolder
        );

        // ----------------------------------------------------
        // LOAD CSPICE KERNELS
        // ----------------------------------------------------

        furnsh_c(
            leapsecondsKernel.string().c_str()
        );

        furnsh_c(
            ephemerisKernel.string().c_str()
        );

        // ----------------------------------------------------
        // WRITE COVER MANIFEST DATABASE
        // ----------------------------------------------------

        const fs::path coversCsvPath =
            sessionFolder /
            "covers.csv";

        writeCoversCsv(
            coversCsvPath,
            covers
        );

        // ----------------------------------------------------
        // OPEN EXPERIMENT DATABASE
        // ----------------------------------------------------

        const fs::path experimentsCsvPath =
            sessionFolder /
            "experiments.csv";

        std::ofstream experimentFile(
            experimentsCsvPath
        );

        if (!experimentFile)
        {
            throw std::runtime_error(
                "Could not create experiments.csv"
            );
        }

        experimentFile
            << std::fixed
            << std::setprecision(8);

        writeExperimentCsvHeader(
            experimentFile
        );

        // ----------------------------------------------------
        // PREPARE RANDOMIZED / BALANCED INPUTS
        // ----------------------------------------------------

        PlanetPairRotator pairRotator(
            rng
        );

        std::unordered_set<std::string>
            usedTimes;

        std::unordered_set<std::uint64_t>
            usedMessageNumbers;

        std::uniform_int_distribution<std::uint64_t>
            messageNumberDistribution(
                1ULL,
                4000000000ULL
            );

        // Five balanced payload-length bands.
        const std::vector<
            std::pair<std::size_t, std::size_t>
        > lengthBands =
        {
            {250, 299},
            {300, 349},
            {350, 399},
            {400, 449},
            {450, 500}
        };

        std::vector<ExperimentResult>
            results;

        results.reserve(
            totalExperiments
        );

        std::size_t runNumber = 0;

        // ----------------------------------------------------
        // TEST EVERY COVER
        // ----------------------------------------------------

        for (
            std::size_t coverIndex = 0;
            coverIndex < covers.size();
            ++coverIndex
        )
        {
            const fs::path coverPath =
                covers[coverIndex];

            const std::string coverText =
                readTextFile(
                    coverPath
                );

            const TextStatistics originalStats =
                analyzeText(
                    coverText
                );

            const std::string category =
                inferCategory(
                    coverPath
                );

            for (
                std::size_t repeat = 0;
                repeat < runsPerCover;
                ++repeat
            )
            {
                ++runNumber;

                const auto started =
                    std::chrono::steady_clock::now();

                ExperimentResult result;

                result.runNumber =
                    runNumber;

                result.coverFile =
                    coverPath.filename().string();

                result.category =
                    category;

                result.originalStats =
                    originalStats;

                // --------------------------------------------
                // BALANCED MESSAGE LENGTH
                // --------------------------------------------

                const std::size_t bandIndex =
                    (runNumber - 1) %
                    lengthBands.size();

                std::uniform_int_distribution<std::size_t>
                    lengthDistribution(
                        lengthBands[bandIndex].first,
                        lengthBands[bandIndex].second
                    );

                const std::size_t requestedLength =
                    lengthDistribution(
                        rng
                    );

                const std::string message =
                    generateUniqueTestMessage(
                        requestedLength,
                        rng,
                        static_cast<std::uint64_t>(
                            runNumber
                        )
                    );

                result.messageLength =
                    message.size();

                result.payloadBits =
                    32 +
                    message.size() * 8;

                if (
                    result.payloadBits >
                    originalStats.eligibleCharacters
                )
                {
                    throw std::runtime_error(
                        "Cover capacity failure for " +
                        coverPath.filename().string() +
                        ". Eligible positions: " +
                        std::to_string(
                            originalStats.eligibleCharacters
                        ) +
                        ", bits required: " +
                        std::to_string(
                            result.payloadBits
                        )
                    );
                }

                result.capacityUsePercent =
                    calculatePercent(
                        result.payloadBits,
                        originalStats.eligibleCharacters
                    );

                // --------------------------------------------
                // UNIQUE MESSAGE NUMBER
                // --------------------------------------------

                for (;;)
                {
                    const std::uint64_t candidate =
                        messageNumberDistribution(
                            rng
                        );

                    if (
                        usedMessageNumbers.insert(
                            candidate
                        ).second
                    )
                    {
                        result.messageNumber =
                            candidate;

                        break;
                    }
                }

                // --------------------------------------------
                // UNIQUE RANDOM DATE/TIME
                // --------------------------------------------

                result.utcTime =
                    generateRandomUtcTime(
                        rng,
                        usedTimes
                    );

                SpiceDouble et;

                str2et_c(
                    result.utcTime.c_str(),
                    &et
                );

                // --------------------------------------------
                // ROTATING PLANET PAIR
                // --------------------------------------------

                const PlanetPair pair =
                    pairRotator.next();

                result.planet1 =
                    pair.first;

                result.planet2 =
                    pair.second;

                result.distance1Km =
                    getPlanetDistance(
                        result.planet1.c_str(),
                        et
                    );

                result.distance2Km =
                    getPlanetDistance(
                        result.planet2.c_str(),
                        et
                    );

                // --------------------------------------------
                // DERIVE CELESTIAL SEED
                // --------------------------------------------

                result.celestialSeed =
                    deriveCelestialSeed(
                        result.distance1Km,
                        result.distance2Km,
                        result.messageNumber
                    );

                // --------------------------------------------
                // ENCODE
                // --------------------------------------------

                const std::string encoded =
                    encodeMessage(
                        coverText,
                        message,
                        result.celestialSeed
                    );

                result.encodedStats =
                    analyzeText(
                        encoded
                    );

                result.mutationStats =
                    analyzeMutations(
                        coverText,
                        encoded
                    );

                result.coverModificationPercent =
                    calculatePercent(
                        result.mutationStats.changedCharacters,
                        originalStats.totalCharacters
                    );

                result.eligibleModificationPercent =
                    calculatePercent(
                        result.mutationStats.changedCharacters,
                        originalStats.eligibleCharacters
                    );

                // --------------------------------------------
                // CORRECT-SEED DECODE TEST
                // --------------------------------------------

                const std::string recovered =
                    decodeMessage(
                        encoded,
                        result.celestialSeed
                    );

                result.decodePass =
                    recovered ==
                    message;

                // --------------------------------------------
                // DETERMINISTIC REPEAT TEST
                // --------------------------------------------

                const std::string repeatedEncoding =
                    encodeMessage(
                        coverText,
                        message,
                        result.celestialSeed
                    );

                result.deterministicRepeatPass =
                    repeatedEncoding ==
                    encoded;

                // --------------------------------------------
                // WRONG-SEED NEGATIVE CONTROL
                // --------------------------------------------

                result.wrongSeed =
                    result.celestialSeed ^
                    0xA5A5A5A5A5A5A5A5ULL;

                if (
                    result.wrongSeed ==
                    result.celestialSeed
                )
                {
                    ++result.wrongSeed;
                }

                result.wrongSeedRejected =
                    wrongSeedFailsToRecover(
                        encoded,
                        message,
                        result.wrongSeed
                    );

                // --------------------------------------------
                // SAVE MESSAGE + ENCODED SAMPLE
                // --------------------------------------------

                std::ostringstream stem;

                stem
                    << "run_"
                    << std::setw(4)
                    << std::setfill('0')
                    << runNumber;

                const fs::path messagePath =
                    messagesFolder /
                    (
                        stem.str() +
                        "_message.txt"
                    );

                const fs::path encodedPath =
                    encodedFolder /
                    (
                        stem.str() +
                        "_encoded.txt"
                    );

                writeTextFile(
                    messagePath,
                    message
                );

                writeTextFile(
                    encodedPath,
                    encoded
                );

                result.messageFile =
                    fs::relative(
                        messagePath,
                        sessionFolder
                    ).generic_string();

                result.encodedFile =
                    fs::relative(
                        encodedPath,
                        sessionFolder
                    ).generic_string();

                // --------------------------------------------
                // RUNTIME
                // --------------------------------------------

                const auto finished =
                    std::chrono::steady_clock::now();

                result.runtimeMilliseconds =
                    std::chrono::duration<
                        double,
                        std::milli
                    >(
                        finished -
                        started
                    ).count();

                // --------------------------------------------
                // WRITE DATABASE ROW IMMEDIATELY
                // --------------------------------------------
                //
                // Writing each row immediately protects most
                // completed results if a later run fails.
                //

                writeExperimentCsvRow(
                    experimentFile,
                    result
                );

                experimentFile.flush();

                results.push_back(
                    result
                );

                // --------------------------------------------
                // PROGRESS DISPLAY
                // --------------------------------------------

                if (
                    runNumber == 1 ||
                    runNumber %
                        std::max<std::size_t>(
                            1,
                            totalExperiments / 20
                        )
                        == 0 ||
                    runNumber ==
                        totalExperiments
                )
                {
                    const double progress =
                        calculatePercent(
                            runNumber,
                            totalExperiments
                        );

                    std::cout
                        << "Progress: "
                        << runNumber
                        << " / "
                        << totalExperiments
                        << "  ("
                        << std::fixed
                        << std::setprecision(1)
                        << progress
                        << "%)"
                        << "\n";
                }
            }
        }

        experimentFile.close();

        // ----------------------------------------------------
        // SESSION SUMMARY
        // ----------------------------------------------------

        const fs::path summaryPath =
            sessionFolder /
            "session_summary.txt";

        writeSummary(
            summaryPath,
            masterSeed,
            covers.size(),
            runsPerCover,
            results
        );

        // ----------------------------------------------------
        // FINAL CONSOLE SUMMARY
        // ----------------------------------------------------

        std::size_t decodePasses = 0;
        std::size_t repeatPasses = 0;
        std::size_t wrongSeedPasses = 0;

        for (
            const ExperimentResult& result :
            results
        )
        {
            if (result.decodePass)
            {
                ++decodePasses;
            }

            if (
                result.deterministicRepeatPass
            )
            {
                ++repeatPasses;
            }

            if (
                result.wrongSeedRejected
            )
            {
                ++wrongSeedPasses;
            }
        }

        std::cout
            << "\n\nSESSION COMPLETE\n"
            << "==========================================\n";

        std::cout
            << "Covers tested:               "
            << covers.size()
            << "\n";

        std::cout
            << "Experiments completed:       "
            << results.size()
            << "\n";

        std::cout
            << "Correct-seed decode passes:  "
            << decodePasses
            << " / "
            << results.size()
            << "\n";

        std::cout
            << "Repeatability passes:        "
            << repeatPasses
            << " / "
            << results.size()
            << "\n";

        std::cout
            << "Wrong-seed rejection passes: "
            << wrongSeedPasses
            << " / "
            << results.size()
            << "\n";

        std::cout
            << "\nResearch database created at:\n"
            << sessionFolder.string()
            << "\n";

        std::cout
            << "\nFiles created:\n"
            << "  experiments.csv\n"
            << "  covers.csv\n"
            << "  session_summary.txt\n"
            << "  messages/\n"
            << "  encoded/\n";

        std::cout
            << "\nIMPORTANT:\n"
            << "These results measure implementation behavior.\n"
            << "They do not prove cryptographic security or "
            << "undetectability.\n";

        // ----------------------------------------------------
        // CLEAN UP CSPICE
        // ----------------------------------------------------

        kclear_c();

        if (
            decodePasses !=
                results.size() ||
            repeatPasses !=
                results.size()
        )
        {
            return 1;
        }
    }
    catch (
        const std::exception& error
    )
    {
        std::cerr
            << "\nERROR:\n"
            << error.what()
            << "\n";

        kclear_c();

        return 1;
    }

    return 0;
}
