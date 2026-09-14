# Celestial-Dyslexic Protocol Experiment

## About This Project

This project started as an idea I had while learning more about cybersecurity, steganography, and C++. I wanted to see if I could take something completely unrelated to normal encryption, like planetary distance data, and use it as part of a repeatable way to hide information inside ordinary-looking text.

I am still a student, so I did not start this project expecting to create a new encryption standard or something that is automatically secure. My goal was to see if the idea could actually work, get more practice coding in C++, learn how to use an external scientific library, and build something I could test instead of only talking about on paper.

The result is a working proof of concept that uses NASA/JPL SPICE data to calculate planetary distances, derives a deterministic seed from those distances, and uses that seed to control where hidden data is placed inside a larger piece of text.

The project currently focuses on text-based steganography using controlled `b/d` and `p/q` substitutions with a deterministic seed-based position shuffling method that I refer to as **dyslexic jitter**.

> **Important:** CDP is an experimental steganography project, not a replacement for established cryptography. The planetary inputs are public and are not treated as secret key material.

---

## Project Status

The project is currently divided into two phases:

### Phase 1 — Proof of Concept

Phase 1 established that the basic encode/decode pipeline works.

The program can:

- calculate real planetary distances using NASA/JPL CSPICE
- derive a deterministic celestial seed
- convert a hidden message into binary
- shuffle eligible carrier positions using the seed
- embed the message using `b/d` and `p/q` substitutions
- recover the hidden message using the same seed
- demonstrate that changing `messageNum` changes the final embedding pattern

### Phase 2 — Testing & Detection Toolkit

Phase 2 expands the project from a single proof-of-concept test into a larger experimental evaluation.

A separate C++ testing toolkit now performs automated batch experiments across multiple:

- cover texts
- hidden messages
- payload lengths
- message numbers
- UTC dates and times
- planet pairs
- celestial seeds

The first large Phase 2 run completed **500 experiments across 100 synthetic cover texts**.

---

## Quick Start

The repository does **not** include the third-party CSPICE toolkit or the large SPICE kernel files. A setup script is included so the project can prepare those dependencies without hard-coded personal paths.

On Windows:

1. Clone or download the repository.
2. Run `setup_dependencies.bat`.
3. Wait for CSPICE and the required SPICE kernels to download.
4. Open `CDP-Experiment.slnx` in Visual Studio.
5. Build the **x64** configuration.
6. Run the program.

The setup script downloads the official NAIF/JPL Windows CSPICE package plus:

- `naif0012.tls`
- `de440.bsp`

Those downloaded folders are ignored by Git.

If Visual Studio asks to retarget the project to an installed C++ toolset, select the toolset installed with your Visual Studio C++ workload.

---

## How Phase 1 Works

The current proof-of-concept follows this general process:

```text
NASA/JPL planetary data
        ↓
CSPICE calculates planetary distances
        ↓
Two planetary distances are mathematically combined
        ↓
messageNum is mixed into the result
        ↓
A deterministic seed is created
        ↓
The seed shuffles eligible positions in a cover text
        ↓
A secret message is converted into bits
        ↓
Bits are embedded using controlled letter substitutions
        ↓
Encoded cover text is created
        ↓
The same seed reproduces the same position order
        ↓
The hidden message is recovered
```

The important implementation property is that the same agreed-upon inputs reproduce the same position order.

This is useful for deterministic placement, but it should not be confused with cryptographic secrecy.

---

## Why I Used NASA/JPL CSPICE

I wanted the planetary calculations to use real astronomical data instead of placeholder values.

I linked the C version of the NASA/JPL SPICE Toolkit (CSPICE) into the Visual Studio C++ project.

The two main kernel files used in the project are:

- `naif0012.tls` — leap-second information
- `de440.bsp` — planetary ephemeris data

The program loads these kernels and calculates the position of a target body relative to Earth for a specific date and time.

For the original proof-of-concept test, I used:

```text
MARS BARYCENTER
JUPITER BARYCENTER
```

CSPICE returns the X, Y, and Z position components. The program then uses `vnorm_c()` to convert that three-dimensional position vector into one straight-line distance in kilometers.

One Phase 1 test produced approximately:

```text
Earth to Mars:
266,934,430.70 km

Earth to Jupiter:
913,875,674.78 km
```

---

## Building the Celestial Seed

The program currently uses the relationship between two planetary distances to create a deterministic starting value.

For the original test:

```text
Mars distance / Jupiter distance
```

The program then:

1. Calculates the ratio between the two distances.
2. Multiplies the ratio by `1,000,000`.
3. Multiplies that value by the primary planet distance.
4. Reduces the result into a manageable numeric range.
5. XORs the result with `messageNum`.

Example:

```text
same planets
same date
same cover
same hidden message

messageNum = 1
        ↓
Seed A

messageNum = 2
        ↓
Seed B
```

Changing `messageNum` changes the final seed and therefore changes the shuffled embedding positions.

### Security Note

The celestial seed is **not a cryptographic key**.

Planetary positions are public information and can be reproduced by anyone using the same inputs and ephemeris data.

I treat the celestial portion as an experimental deterministic input / obfuscation mechanism, not as proven cryptographic security.

---

## The Text Steganography Method

The current experiment uses these substitution pairs:

```text
b ↔ d
p ↔ q
```

The bit mapping is:

```text
b = 0
p = 0

d = 1
q = 1
```

The program scans the cover text and records every position containing one of those four letters.

Those positions become possible locations for hidden bits.

---

## Dyslexic Jitter

Instead of always embedding bits in the first available eligible positions, the program uses the celestial seed with C++'s `std::mt19937_64` pseudorandom number generator.

It then runs:

```cpp
std::shuffle(...)
```

on the eligible-position list.

This gives the program a deterministic shuffled order:

```text
Same seed
    ↓
Same shuffled positions

Different seed
    ↓
Different shuffled positions
```

I refer to this deterministic position-selection behavior as **dyslexic jitter**.

---

## Message Format

The hidden message is converted into:

```text
[32-bit message length][message bytes]
```

Each normal message character requires 8 bits.

So the required number of carrier positions is:

```text
32 + (message characters × 8)
```

For the original 291-character Phase 1 message:

```text
Message characters: 291
Bits required:       2360
```

---

## Phase 1 Capacity Test

The original Phase 1 cover contained approximately:

```text
Cover length:         90,346 characters
Eligible positions:   12,679
Required positions:    2,360
```

Result:

```text
Capacity result: PASS
```

The current system can only store one bit in each eligible `b`, `d`, `p`, or `q` position, so relatively large cover texts are required.

---

## Phase 1 Jitter Test

I tested whether changing only `messageNum` would produce a different encoded cover.

Everything else stayed the same:

```text
same hidden message
same cover text
same planets
same date
```

The only change was:

```text
messageNum = 1
```

versus:

```text
messageNum = 2
```

The two encoded outputs were then compared character-by-character.

Results:

```text
Characters compared: 90,346
Different positions:  2,115
Difference percent:   2.3410%

RESULT: PASS
```

Both versions still recovered the same original hidden message.

The encoded covers changed 1,159 and 1,166 characters respectively when compared with the original cover. The larger 2,115-position value is the direct comparison between the two encoded outputs.

---

# Phase 2: Testing & Detection Toolkit

After completing the first proof of concept, I built a separate C++ testing toolkit to evaluate CDP under a larger number of controlled conditions.

The toolkit is stored in:

```text
testing-toolkit/
```

The purpose of Phase 2 is not to prove that CDP is secure. It is to measure how the current design behaves, identify weaknesses, and generate data that can later be used for steganalysis and detector development.

---

## Phase 2 Experimental Variables

The toolkit varies:

- cover text
- hidden message
- payload length
- message number
- UTC date and time
- planet pair
- calculated planetary distances
- celestial seed

Each experiment generates a new test message and evaluates a different set of conditions.

The first large run used:

```text
100 synthetic cover texts
5 runs per cover
500 total experiments
```

The synthetic dataset is included under:

```text
dataset/synthetic-covers/
```

---

## Phase 2 Validation Checks

Every experiment performs three major validation checks.

### 1. Correct-Seed Recovery

The encoded message is decoded using the same seed used during embedding.

Expected result:

```text
Recovered message = original hidden message
```

### 2. Deterministic Repeatability

The exact same experiment is encoded again.

Expected result:

```text
Same inputs = same encoded output
```

### 3. Wrong-Seed Negative Control

The program intentionally attempts to recover the message using an incorrect seed.

Expected result:

```text
Wrong seed should not successfully recover the original message
```

These checks are intended to verify implementation behavior, not cryptographic security.

---

## 500-Experiment Results

The first full Phase 2 batch produced:

```text
Cover files tested:                  100
Runs per cover:                        5
Total experiments:                   500

Correct-seed decode passes:      500 / 500
Deterministic repeat passes:     500 / 500
Wrong-seed rejection passes:     500 / 500

Mean cover modification:          2.204685%
Median cover modification:        2.176294%
Mean eligible-position change:   27.978514%
Mean capacity utilization:       55.948578%
Mean changed words:            1404.080000
Mean runtime per experiment:      12.305642 ms
```

The complete CSV results and session summary are stored in:

```text
results/
├── phase2_500_experiments.csv
└── phase2_500_summary.txt
```

---

## What the 500-Run Test Demonstrated

Under the tested conditions:

- all 500 correct-seed recovery tests passed
- all 500 deterministic repeat tests passed
- all 500 wrong-seed negative controls rejected the incorrect seed
- average cover modification stayed near 2.2%
- average use of available eligible positions was approximately 56%
- the testing toolkit successfully generated repeatable experimental data across many different combinations of inputs

These results demonstrate that the current implementation behaved consistently under the tested conditions.

They do **not** establish:

- cryptographic security
- resistance to steganalysis
- natural-language stealth
- cross-platform deterministic compatibility
- safe real-world communication

---

## Early Detectability Findings

Phase 2 also revealed an important weakness.

Because CDP directly substitutes:

```text
b ↔ d
p ↔ q
```

the resulting text can create malformed words such as:

```text
comdine
shoulb
exqlicit
```

This suggests that the current version may be detectable using relatively simple linguistic or statistical features.

That is useful research evidence rather than a reason to hide the result. One of the main goals of the next phase is to measure exactly how detectable the current system is.

Potential detector features include:

- `b/d` balance
- `p/q` balance
- unusual `q` frequency
- eligible-character entropy
- suspicious substitution patterns
- misspelling rate
- malformed-word frequency
- comparison between original and encoded character distributions

---

## Current Research Limitations

### 1. Public Astronomical Data

Planetary values are reproducible and should not be treated as secret cryptographic material.

### 2. Synthetic Dataset

The current 100-cover Phase 2 dataset is synthetic.

That makes it useful for controlled testing, but it is not enough to make strong claims about performance on naturally written text.

Future testing should include:

- public-domain writing
- openly licensed text
- original human-written samples
- multiple writing styles and authors

### 3. Visible Word Corruption

The current direct letter-substitution method can create obvious spelling errors and malformed words.

This is likely one of the largest detectability weaknesses in the current design.

### 4. Limited Carrier Alphabet

Only four letters currently carry data:

```text
b
d
p
q
```

This limits capacity and creates concentrated statistical changes.

### 5. Large Cover Requirement

A relatively small payload requires a much larger carrier text.

### 6. No Payload Encryption Yet

The current proof of concept focuses on hiding and recovering data.

It does not yet encrypt the payload before embedding.

A future secure architecture should use established authenticated encryption before steganographic embedding.

### 7. Cross-Platform Reproducibility Has Not Been Proven

The current implementation was developed and tested using Visual Studio on Windows.

The seed is deterministic, but exact `std::shuffle` behavior can depend on the C++ standard-library implementation.

I have not yet demonstrated that two different compilers or standard-library implementations will reproduce the exact same shuffled order.

---

## What This Project Is Not

CDP is not:

- a replacement for AES or other established cryptography
- a proven secure communication system
- proven undetectable
- proven resistant to professional steganalysis
- a production-ready security product

The current project is an experimental steganography and testing platform.

If confidentiality is added later, I plan to use established encryption rather than inventing my own encryption algorithm.

A future design could look like:

```text
plaintext
    ↓
optional compression
    ↓
authenticated encryption
    ↓
ciphertext
    ↓
CDP embedding
    ↓
carrier text
```

In that design:

```text
Encryption
= protects the contents of the message

CDP
= attempts to conceal or obscure where the payload is carried
```

---

## Repository Structure

```text
.
├── README.md
├── .gitignore
├── Main.cpp
├── CDP-Experiment.slnx
├── CDP-Experiment.vcxproj
├── CDP-Experiment.vcxproj.filters
├── setup_dependencies.bat
├── setup_dependencies.ps1
├── secret.txt
├── cover.txt
├── encoded_cover_msg1.txt
├── encoded_cover_msg2.txt
│
├── testing-toolkit/
│   └── Main.cpp
│
├── results/
│   ├── phase2_500_experiments.csv
│   └── phase2_500_summary.txt
│
└── dataset/
    └── synthetic-covers/
        └── 100 synthetic cover text files
```

Generated Phase 2 session folders are intentionally not committed.

The `.gitignore` excludes:

```text
toolkit_results/
```

along with local Visual Studio build files, CSPICE files, downloaded SPICE kernels, and other reproducible output files.

---

## Building the Project

### Requirements

Current development environment:

- Windows
- Visual Studio
- C++
- NASA/JPL CSPICE Toolkit
- DE440 planetary ephemeris kernel
- NAIF leap-second kernel

### CSPICE Setup

CSPICE and the SPICE kernels are not committed to the repository because they are third-party dependencies and `de440.bsp` is larger than GitHub's normal single-file limit.

For Windows, run:

```text
setup_dependencies.bat
```

The script downloads the 64-bit Windows CSPICE toolkit and places the required files under:

```text
cspice/include/
cspice/lib/
```

It also downloads:

```text
kernels/naif0012.tls
kernels/de440.bsp
```

The Visual Studio project uses relative paths so personal Windows paths are not required.

The main C++ source includes CSPICE using:

```cpp
extern "C"
{
#include "SpiceUsr.h"
}
```

The current project is built using the **x64** configuration.

---

## What I Learned

This project has given me practice with:

- C++
- file input/output
- external C libraries
- Visual Studio project configuration
- binary representation
- deterministic pseudorandom generation
- vector manipulation
- text processing
- data validation
- CSV output
- automated testing
- negative controls
- experimental design
- debugging
- separating implementation success from security claims

One of the biggest lessons has been that something can be technically interesting and repeatable without automatically being secure.

The testing phase also reinforced that measuring weaknesses is just as important as demonstrating that the program works.

---

## Next Steps

The next major phase is focused on **detection and stronger experimental controls**.

Planned work includes:

- build **CDP Detector v0.1**
- compare the 100 original covers against the 500 encoded samples
- measure false-positive and false-negative rates
- test `b/d` and `p/q` balance features
- measure unusual `q` frequency
- measure eligible-character entropy
- detect malformed-word and substitution patterns
- add naturally written control texts
- compare results across different writing styles
- improve generated test-message sentence endings
- investigate alternatives that reduce obvious word corruption
- test cross-platform reproducibility
- eventually add authenticated encryption before embedding

For any future encryption layer, the goal is to use established cryptographic standards rather than custom encryption.

---

## Why I Built It

The main reason I built CDP was curiosity.

I wanted to see whether real planetary movement could be turned into a repeatable value and then used to control a text-steganography experiment.

I also wanted a project that would force me to practice more than basic classroom C++.

Phase 1 established that the encode/decode pipeline works.

Phase 2 gave me a larger testing framework and measurable results across 500 experiments.

The next question is no longer just:

```text
Can the program hide and recover a message?
```

It is:

```text
How detectable is the current method,
under what conditions does it fail,
and what can those failures teach me?
```

That is the direction I want to continue researching.
