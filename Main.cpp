#include <iostream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <cctype>
#include <stdexcept>

// ============================================================
// CSPICE SETUP
// ============================================================
//
// CSPICE is written in C.
// extern "C" allows our C++ program to call CSPICE functions.
//
extern "C"
{
#include "SpiceUsr.h"
}


// ============================================================
// 1. GET PLANET DISTANCE
// ============================================================
//
// CSPICE gives us the X/Y/Z position of a planet relative
// to Earth.
//
// vnorm_c() converts that X/Y/Z vector into one straight-line
// distance in kilometers.
//
SpiceDouble getPlanetDistance(
    const char* target,
    SpiceDouble et
)
{
    SpiceDouble position[3];
    SpiceDouble lightTime;

    spkpos_c(
        target,         // Planet/body we want
        et,             // Time
        "J2000",        // Reference frame
        "NONE",         // No light-time correction
        "EARTH",        // Observer
        position,       // X/Y/Z result
        &lightTime
    );

    return vnorm_c(position);
}


// ============================================================
// 2. REDUCE LARGE CELESTIAL NUMBER
// ============================================================
//
// Some celestial calculations create extremely large numbers.
//
// This reduces the number to a manageable range.
//
// IMPORTANT:
// This is NOT a cryptographic hash.
// It is only part of the experimental celestial seed method.
//
std::uint64_t reduceToSeedRange(long double value)
{
    const long double MODULUS = 1000000007.0L;

    long double reduced =
        std::fmod(
            std::fabs(value),
            MODULUS
        );

    return static_cast<std::uint64_t>(reduced);
}


// ============================================================
// 3. CELESTIAL SEED
// ============================================================
//
// Current experiment uses:
//
// Mars distance / Jupiter distance
//
// Then:
//
// ratio
//   ↓
// scale ratio
//   ↓
// combine with Mars distance
//   ↓
// reduce number
//   ↓
// XOR message number
//   ↓
// final seed
//
std::uint64_t deriveCelestialSeed(
    SpiceDouble distance1,
    SpiceDouble distance2,
    std::uint64_t messageNum
)
{
    // Relationship between Planet 1 and Planet 2.
    long double ratio =
        static_cast<long double>(distance1) /
        static_cast<long double>(distance2);

    // Preserve more decimal information.
    long double scaledRatio =
        ratio * 1000000.0L;

    // Mix primary planet distance with the ratio.
    long double combined =
        static_cast<long double>(distance1) *
        scaledRatio;

    // Reduce the huge result.
    std::uint64_t celestialValue =
        reduceToSeedRange(combined);

    // XOR the message counter into the result.
    //
    // messageNum = 1
    // and
    // messageNum = 2
    //
    // produce different seeds.
    return celestialValue ^ messageNum;
}


// ============================================================
// 4. READ TEXT FILE
// ============================================================
//
// Reads an entire text file into one std::string.
//
// Used for:
//
// secret.txt
// cover.txt
// encoded_cover_msg1.txt
// encoded_cover_msg2.txt
//
std::string readTextFile(
    const std::string& path
)
{
    std::ifstream file(
        path,
        std::ios::binary
    );

    if (!file)
    {
        throw std::runtime_error(
            "Could not open file: " + path
        );
    }

    std::string contents(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    return contents;
}


// ============================================================
// 5. WRITE TEXT FILE
// ============================================================
//
// Used to create:
//
// encoded_cover.txt
// recovered.txt
//
void writeTextFile(
    const std::string& path,
    const std::string& text
)
{
    std::ofstream file(
        path,
        std::ios::binary
    );

    if (!file)
    {
        throw std::runtime_error(
            "Could not create file: " + path
        );
    }

    file.write(
        text.data(),
        static_cast<std::streamsize>(text.size())
    );
}


// ============================================================
// 6. CHECK IF CHARACTER CAN HOLD A CDP BIT
// ============================================================
//
// Current experimental letter pairs:
//
// b <-> d
// p <-> q
//
// These letters act as our possible carrier positions.
//
bool isEligibleCharacter(char character)
{
    char lower =
        static_cast<char>(
            std::tolower(
                static_cast<unsigned char>(character)
            )
            );

    return (
        lower == 'b' ||
        lower == 'd' ||
        lower == 'p' ||
        lower == 'q'
        );
}


// ============================================================
// 7. FIND ALL ELIGIBLE POSITIONS
// ============================================================
//
// Goes through cover.txt and records the location of every:
//
// b
// d
// p
// q
//
// These positions are later shuffled by the celestial seed.
//
std::vector<std::size_t> findEligiblePositions(
    const std::string& text
)
{
    std::vector<std::size_t> positions;

    for (std::size_t i = 0;
        i < text.size();
        ++i)
    {
        if (isEligibleCharacter(text[i]))
        {
            positions.push_back(i);
        }
    }

    return positions;
}


// ============================================================
// 8. CONVERT SECRET MESSAGE INTO BITS
// ============================================================
//
// Format:
//
// [32-bit message length][message bytes]
//
// The first 32 bits tell the decoder how many characters
// are hidden.
//
// Each normal message character then requires 8 bits.
//
std::vector<int> messageToBits(
    const std::string& message
)
{
    std::vector<int> bits;

    if (message.size() > UINT32_MAX)
    {
        throw std::runtime_error(
            "Message is too large."
        );
    }

    std::uint32_t messageLength =
        static_cast<std::uint32_t>(
            message.size()
            );


    // --------------------------------------------------------
    // Store message length using 32 bits.
    // --------------------------------------------------------

    for (int bit = 31; bit >= 0; --bit)
    {
        bits.push_back(
            (messageLength >> bit) & 1
        );
    }


    // --------------------------------------------------------
    // Store every character using 8 bits.
    // --------------------------------------------------------

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
// 9. WRITE ONE HIDDEN BIT INTO A CHARACTER
// ============================================================
//
// Mapping:
//
// b = 0
// d = 1
//
// p = 0
// q = 1
//
// Example:
//
// original letter = b
//
// bit 0 -> b
// bit 1 -> d
//
char writeBit(
    char original,
    int bit
)
{
    bool uppercase =
        std::isupper(
            static_cast<unsigned char>(original)
        );

    char lower =
        static_cast<char>(
            std::tolower(
                static_cast<unsigned char>(original)
            )
            );

    char result = original;


    // b / d family
    if (lower == 'b' || lower == 'd')
    {
        result =
            (bit == 0)
            ? 'b'
            : 'd';
    }


    // p / q family
    else if (lower == 'p' || lower == 'q')
    {
        result =
            (bit == 0)
            ? 'p'
            : 'q';
    }


    // Preserve uppercase if needed.
    if (uppercase)
    {
        result =
            static_cast<char>(
                std::toupper(
                    static_cast<unsigned char>(result)
                )
                );
    }

    return result;
}


// ============================================================
// 10. READ ONE HIDDEN BIT
// ============================================================
//
// Reverse mapping:
//
// b = 0
// p = 0
//
// d = 1
// q = 1
//
int readBit(char character)
{
    char lower =
        static_cast<char>(
            std::tolower(
                static_cast<unsigned char>(character)
            )
            );

    if (lower == 'b' || lower == 'p')
    {
        return 0;
    }

    if (lower == 'd' || lower == 'q')
    {
        return 1;
    }

    return -1;
}


// ============================================================
// 11. ENCODE MESSAGE INTO COVER TEXT
// ============================================================
//
// PROCESS:
//
// secret.txt
//      ↓
// convert message to bits
//      ↓
// find b/d/p/q locations
//      ↓
// celestial seed
//      ↓
// shuffle positions
//      ↓
// write secret bits
//
// The shuffled position order is our current implementation
// of DYSLEXIC JITTER.
//
std::string encodeMessage(
    const std::string& coverText,
    const std::string& message,
    std::uint64_t seed
)
{
    std::vector<int> bits =
        messageToBits(message);

    std::vector<std::size_t> positions =
        findEligiblePositions(coverText);


    std::cout
        << "\nCAPACITY CHECK\n"
        << "====================================\n";

    std::cout
        << "Message characters: "
        << message.size()
        << "\n";

    std::cout
        << "Bits required:      "
        << bits.size()
        << "\n";

    std::cout
        << "Eligible positions: "
        << positions.size()
        << "\n";


    // Stop if cover.txt is too small.
    if (bits.size() > positions.size())
    {
        throw std::runtime_error(
            "Cover text does not contain enough "
            "b/d/p/q positions."
        );
    }


    std::cout
        << "Capacity result:    PASS\n";


    // ========================================================
    // DYSLEXIC JITTER
    // ========================================================
    //
    // Same seed = same shuffled position order.
    //
    // Different seed = different shuffled position order.
    //
    std::mt19937_64 randomGenerator(seed);

    std::shuffle(
        positions.begin(),
        positions.end(),
        randomGenerator
    );


    std::string encodedText =
        coverText;


    // Write one secret bit into each selected position.
    for (std::size_t i = 0;
        i < bits.size();
        ++i)
    {
        std::size_t selectedPosition =
            positions[i];

        encodedText[selectedPosition] =
            writeBit(
                encodedText[selectedPosition],
                bits[i]
            );
    }

    return encodedText;
}


// ============================================================
// 12. DECODE MESSAGE FROM COVER TEXT
// ============================================================
//
// Receiver recreates the SAME shuffled position order using
// the same seed.
//
// First:
//
// recover 32-bit message length.
//
// Then:
//
// recover the message itself.
//
std::string decodeMessage(
    const std::string& encodedText,
    std::uint64_t seed
)
{
    std::vector<std::size_t> positions =
        findEligiblePositions(encodedText);


    if (positions.size() < 32)
    {
        throw std::runtime_error(
            "Not enough positions to read message header."
        );
    }


    // Reproduce the same dyslexic jitter.
    std::mt19937_64 randomGenerator(seed);

    std::shuffle(
        positions.begin(),
        positions.end(),
        randomGenerator
    );


    // ========================================================
    // READ 32-BIT MESSAGE LENGTH
    // ========================================================

    std::uint32_t messageLength = 0;

    for (int i = 0; i < 32; ++i)
    {
        int bit =
            readBit(
                encodedText[
                    positions[i]
                ]
            );

        messageLength =
            (messageLength << 1)
            |
            static_cast<std::uint32_t>(bit);
    }


    std::cout
        << "\nDecoded message length: "
        << messageLength
        << " characters\n";


    std::size_t requiredBits =
        32 +
        static_cast<std::size_t>(
            messageLength
            ) * 8;


    if (requiredBits > positions.size())
    {
        throw std::runtime_error(
            "Encoded cover does not contain "
            "the complete hidden message."
        );
    }


    // ========================================================
    // RECOVER MESSAGE CHARACTERS
    // ========================================================

    std::string recoveredMessage;

    recoveredMessage.reserve(
        messageLength
    );


    std::size_t positionIndex = 32;


    for (std::uint32_t characterIndex = 0;
        characterIndex < messageLength;
        ++characterIndex)
    {
        unsigned char character = 0;


        for (int bitIndex = 0;
            bitIndex < 8;
            ++bitIndex)
        {
            int bit =
                readBit(
                    encodedText[
                        positions[
                            positionIndex
                        ]
                    ]
                );


            character =
                static_cast<unsigned char>(
                    (character << 1) | bit
                    );


            ++positionIndex;
        }


        recoveredMessage.push_back(
            static_cast<char>(character)
        );
    }

    return recoveredMessage;
}


// ============================================================
// 13. COMPARE TWO ENCODED COVER FILES
// ============================================================
//
// PURPOSE:
//
// Compare:
//
// encoded_cover_msg1.txt
//
// with:
//
// encoded_cover_msg2.txt
//
// character-by-character.
//
// If changing messageNum changes the jitter positions,
// these files should NOT be identical.
//
void compareEncodedCovers(
    const std::string& file1Path,
    const std::string& file2Path
)
{
    std::string file1 =
        readTextFile(file1Path);

    std::string file2 =
        readTextFile(file2Path);


    // Both should come from the same original cover.txt,
    // so they should have exactly the same file length.
    if (file1.size() != file2.size())
    {
        std::cout
            << "\nJITTER COMPARISON ERROR\n"
            << "The encoded files have different lengths.\n";

        return;
    }


    std::size_t differences = 0;


    // Compare every character.
    for (std::size_t i = 0;
        i < file1.size();
        ++i)
    {
        if (file1[i] != file2[i])
        {
            ++differences;
        }
    }


    double differencePercent = 0.0;

    if (!file1.empty())
    {
        differencePercent =
            (
                static_cast<double>(differences)
                /
                static_cast<double>(file1.size())
                )
            * 100.0;
    }


    std::cout
        << "\nJITTER COMPARISON\n"
        << "====================================\n";

    std::cout
        << "Characters compared: "
        << file1.size()
        << "\n";

    std::cout
        << "Different positions:  "
        << differences
        << "\n";

    std::cout
        << std::fixed
        << std::setprecision(4);

    std::cout
        << "Difference percent:   "
        << differencePercent
        << "%\n";


    if (differences > 0)
    {
        std::cout
            << "\nRESULT: PASS\n"
            << "Changing messageNum produced a different "
            << "dyslexic jitter pattern.\n";
    }

    else
    {
        std::cout
            << "\nRESULT: FAIL\n"
            << "The two encoded covers are identical.\n";
    }
}


// ============================================================
// MAIN PROGRAM
// ============================================================
//
// THERE SHOULD ONLY BE ONE int main() IN THIS FILE.
//
int main()
{
    try
    {
        std::cout
            << "CDP CELESTIAL SEED + DYSLEXIC JITTER TEST\n"
            << "====================================\n\n";


        // ====================================================
        // CHECK LOCAL DEPENDENCIES
        // ====================================================
        //
        // CSPICE itself is a build-time dependency. The two kernel
        // files below are runtime dependencies and are intentionally
        // not committed to GitHub. Run setup_dependencies.bat once
        // after cloning/downloading the repository.
        //
        if (
            !std::filesystem::exists("kernels/naif0012.tls") ||
            !std::filesystem::exists("kernels/de440.bsp")
        )
        {
            throw std::runtime_error(
                "Required SPICE kernels are missing. "
                "Run setup_dependencies.bat, then build again."
            );
        }


        // ====================================================
        // LOAD NASA/JPL SPICE KERNELS
        // ====================================================

        furnsh_c(
            "kernels/naif0012.tls"
        );

        furnsh_c(
            "kernels/de440.bsp"
        );


        // ====================================================
        // SET DATE/TIME
        // ====================================================

        SpiceDouble et;

        str2et_c(
            "2026 SEP 12 12:00:00 UTC",
            &et
        );


        // ====================================================
        // CELESTIAL PLANET PAIR
        // ====================================================

        const char* planet1 =
            "MARS BARYCENTER";

        const char* planet2 =
            "JUPITER BARYCENTER";


        SpiceDouble distance1 =
            getPlanetDistance(
                planet1,
                et
            );


        SpiceDouble distance2 =
            getPlanetDistance(
                planet2,
                et
            );


        // ====================================================
        // MESSAGE NUMBER
        // ====================================================
        //
        // Change this to 1 or 2 when creating new tests.
        //
        // Your saved encoded_cover_msg1.txt and
        // encoded_cover_msg2.txt are NOT overwritten by this.
        //
        std::uint64_t messageNum = 2;


        // ====================================================
        // GENERATE CELESTIAL SEED
        // ====================================================

        std::uint64_t seed =
            deriveCelestialSeed(
                distance1,
                distance2,
                messageNum
            );


        std::cout
            << std::fixed
            << std::setprecision(6);


        std::cout
            << "Planet 1: "
            << planet1
            << "\n";

        std::cout
            << "Distance: "
            << distance1
            << " km\n\n";


        std::cout
            << "Planet 2: "
            << planet2
            << "\n";

        std::cout
            << "Distance: "
            << distance2
            << " km\n\n";


        std::cout
            << "Message number: "
            << messageNum
            << "\n";

        std::cout
            << "Celestial seed: "
            << seed
            << "\n";


        // ====================================================
        // READ SECRET MESSAGE
        // ====================================================

        std::string secretMessage =
            readTextFile(
                "secret.txt"
            );


        // Remove final newline if Notepad added one.
        while (
            !secretMessage.empty() &&
            (
                secretMessage.back() == '\n' ||
                secretMessage.back() == '\r'
                )
            )
        {
            secretMessage.pop_back();
        }


        // ====================================================
        // ENFORCE 250-500 CHARACTER TARGET
        // ====================================================

        if (
            secretMessage.size() < 250 ||
            secretMessage.size() > 500
            )
        {
            throw std::runtime_error(
                "secret.txt must contain between "
                "250 and 500 characters."
            );
        }


        // ====================================================
        // LOAD COVER TEXT
        // ====================================================

        std::string coverText =
            readTextFile(
                "cover.txt"
            );


        // ====================================================
        // ENCODE SECRET MESSAGE
        // ====================================================

        std::string encodedCover =
            encodeMessage(
                coverText,
                secretMessage,
                seed
            );


        // Save current run.
        //
        // This does NOT overwrite your saved msg1/msg2 files.
        writeTextFile(
            "encoded_cover.txt",
            encodedCover
        );


        std::cout
            << "\nEncoded cover created:\n"
            << "encoded_cover.txt\n";


        // ====================================================
        // DECODE CURRENT RUN
        // ====================================================

        std::string recoveredMessage =
            decodeMessage(
                encodedCover,
                seed
            );


        writeTextFile(
            "recovered.txt",
            recoveredMessage
        );


        std::cout
            << "\nRecovered message created:\n"
            << "recovered.txt\n";


        // ====================================================
        // VERIFY CURRENT RUN
        // ====================================================

        std::cout
            << "\nVERIFICATION\n"
            << "====================================\n";


        if (secretMessage == recoveredMessage)
        {
            std::cout
                << "SUCCESS\n"
                << "Recovered message matches "
                << "secret.txt exactly.\n";
        }

        else
        {
            std::cout
                << "FAILED\n"
                << "Recovered message does not match "
                << "secret.txt.\n";
        }


        // ====================================================
        // COMPARE SAVED MESSAGE #1 AND MESSAGE #2 COVERS
        // ====================================================
        //
        // We only run this comparison if BOTH files exist.
        //
        // This prevents an error if one of them is missing.
        //

        if (
            std::filesystem::exists(
                "encoded_cover_msg1.txt"
            )
            &&
            std::filesystem::exists(
                "encoded_cover_msg2.txt"
            )
            )
        {
            compareEncodedCovers(
                "encoded_cover_msg1.txt",
                "encoded_cover_msg2.txt"
            );
        }

        else
        {
            std::cout
                << "\nJITTER COMPARISON NOT RUN\n"
                << "encoded_cover_msg1.txt and/or "
                << "encoded_cover_msg2.txt is missing.\n";
        }


        // ====================================================
        // CLEAN UP CSPICE
        // ====================================================

        kclear_c();
    }

    catch (const std::exception& error)
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