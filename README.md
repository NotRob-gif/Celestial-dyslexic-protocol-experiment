Celestial-Dyslexic Protocol Experiment

The Celestial-Dyslexic Protocol (CDP) is a personal C++ cybersecurity project exploring text-based steganography.

The project uses NASA/JPL SPICE data to calculate planetary distances, derives a deterministic seed from those values, and uses that seed to control where hidden data is placed inside a larger piece of text.

CDP currently uses controlled b/d and p/q substitutions with a deterministic position-shuffling method I call dyslexic jitter.

Important: CDP is an experimental steganography project. It is not a replacement for established cryptography, and the planetary inputs are public information rather than secret key material.

Project Status

Phase 1 - Proof of Concept

Status: Complete

The original C++ proof of concept can:

calculate real planetary distances using NASA/JPL CSPICE

derive a deterministic celestial seed

convert a hidden message into binary

find eligible b, d, p, and q positions in a cover text

shuffle those positions using the derived seed

embed the message through controlled letter substitutions

recover the hidden message using the same seed

demonstrate that changing messageNum changes the embedding pattern

Phase 2 - Batch Testing and Measurement

Status: Complete

I built a separate C++ testing toolkit to evaluate CDP across a much larger set of conditions.

The first full Phase 2 evaluation used:

100 synthetic cover texts

5 runs per cover

500 total experiments

Results:

500 / 500 correct-seed recovery passes

500 / 500 deterministic repeatability passes

500 / 500 wrong-seed rejection passes

approximately 2.2% average modification across the full cover text

After updating the toolkit to support the GitHub dataset path, I ran another 500-experiment verification session. It again completed all 500 experiments successfully.

Phase 3 - Detection and Steganalysis

Status: Next

The next step is to build a defensive CDP detector and measure whether CDP-modified text can be distinguished from clean text.

Planned work includes:

testing b/d and p/q balance

measuring unusual q frequency

measuring eligible-character entropy

checking malformed-word and substitution patterns

adding naturally written control texts

measuring false-positive and false-negative rates

Phase 4 - Authenticated Encryption

Status: Planned

The next security layer will use established authenticated encryption before CDP embedding.

The intended design is:

plaintext
    |
    v
authenticated encryption
    |
    v
ciphertext
    |
    v
CDP / dyslexic jitter
    |
    v
carrier text

Encryption will be responsible for protecting the contents of the hidden message. CDP will remain the steganographic placement and concealment layer.

AES-256-GCM is one candidate for the authenticated-encryption layer.

Phase 5 - Optional Post-Quantum Key Establishment

Status: Future research

A later phase may evaluate a standardized post-quantum key-establishment method for securely establishing the symmetric encryption key.

This is future work and is not part of the current CDP implementation.

Quick Start

The repository does not include the third-party CSPICE toolkit or the large SPICE kernel files.

On Windows:

Clone or download the repository.

Run setup_dependencies.bat.

Wait for CSPICE and the required SPICE kernels to download.

Open CDP-Experiment.slnx in Visual Studio.

Build the project using the x64 configuration.

Run the program.

The setup script downloads the required CSPICE files and these SPICE kernels:

naif0012.tls

de440.bsp

These downloaded dependencies are ignored by Git.

How Phase 1 Works

The basic Phase 1 flow is:

NASA/JPL planetary data
        |
        v
CSPICE calculates planetary distances
        |
        v
Two planetary distances are combined
        |
        v
messageNum is mixed into the result
        |
        v
A deterministic seed is created
        |
        v
Eligible text positions are shuffled
        |
        v
The hidden message is converted to bits
        |
        v
Bits are embedded using b/d and p/q substitutions
        |
        v
Encoded carrier text is created
        |
        v
The same inputs reproduce the same position order
        |
        v
The hidden message is recovered

The important property is that the same agreed-upon inputs reproduce the same position order.

That makes the placement deterministic, but it does not make the celestial seed a cryptographic secret.

Why I Used NASA/JPL CSPICE

I wanted the planetary calculations to use real astronomical data instead of placeholder values.

The project uses the C version of the NASA/JPL SPICE Toolkit, CSPICE, with a Visual Studio C++ project.

The main kernel files used are:

naif0012.tls - leap-second information

de440.bsp - planetary ephemeris data

For the original proof-of-concept test, I used:

MARS BARYCENTER

JUPITER BARYCENTER

One Phase 1 test produced approximately:

Earth to Mars:     266,934,430.70 km
Earth to Jupiter:  913,875,674.78 km

Building the Celestial Seed

The program uses the relationship between two planetary distances to create a deterministic starting value.

The current process:

Calculate the ratio between the two distances.

Multiply the ratio by 1,000,000.

Multiply that value by the primary planet distance.

Reduce the result into a manageable numeric range.

XOR the result with messageNum.

Changing messageNum changes the final seed and therefore changes the shuffled embedding positions.

Security Note

The celestial seed is not a cryptographic key.

Planetary positions are public information and can be reproduced by anyone using the same inputs and ephemeris data.

I treat the celestial portion as an experimental deterministic input and obfuscation mechanism, not as proven cryptographic security.

The Text Steganography Method

The current experiment uses these substitution pairs:

b <-> d
p <-> q

The bit mapping is:

b = 0
p = 0

d = 1
q = 1

The program scans the cover text and records every position containing one of those four letters.

Those positions become possible locations for hidden bits.

Dyslexic Jitter

Instead of always embedding bits in the first available eligible positions, CDP uses the celestial seed with C++'s std::mt19937_64 pseudorandom number generator.

The program then shuffles the eligible-position list using std::shuffle.

Same seed
    |
    v
Same shuffled position order

Different seed
    |
    v
Different shuffled position order

I refer to this deterministic position-selection behavior as dyslexic jitter.

Plausible Deniability as a Design Goal

Dyslexic jitter is not meant to be the encryption layer.

The b/d and p/q substitutions carry hidden bits while the shuffled placement changes where those bits appear from one message to another.

The visible changes can resemble ordinary spelling, transcription, or letter-confusion errors. One design goal is that, if the hidden channel is not recognized or successfully extracted, the altered text may have a plausible non-secret explanation.

This is a design goal, not a proven security guarantee. Phase 3 is intended to test how distinguishable CDP-modified text actually is from normal writing.

The planned security model keeps the responsibilities separate:

Encryption protects the contents of the hidden message.

CDP hides and distributes the encrypted payload within the carrier text.

Dyslexic jitter changes the placement pattern and gives the visible substitutions a possible non-secret explanation.

If the hidden payload is successfully extracted after encryption is added, it should still be ciphertext unless the encryption layer is also compromised.

Message Format

The hidden message is currently converted into:

[32-bit message length][message bytes]

Each normal message character requires 8 bits.

Required carrier positions:

32 + (message characters x 8)

For the original 291-character Phase 1 message:

Message characters: 291
Bits required:       2,360

Phase 1 Results

Original cover:

Cover length:       90,346 characters
Eligible positions: 12,679
Required positions:  2,360
Capacity result:     PASS

The Phase 1 jitter test compared two encoded outputs where only messageNum changed.

Characters compared: 90,346
Different positions: 2,115
Difference percent:  2.3410%

Both versions successfully recovered the same original hidden message.

Phase 2 Testing Toolkit

The Phase 2 toolkit is stored under:

testing-toolkit/

It performs automated experiments across different:

cover texts

generated hidden messages

payload lengths

message numbers

UTC dates and times

planet pairs

calculated planetary distances

celestial seeds

The current synthetic dataset is stored under:

dataset/synthetic-covers/

Validation Checks

Each Phase 2 experiment performs three main checks.

1. Correct-Seed Recovery

The encoded message is decoded using the same seed used during embedding.

Expected result:

Recovered message = original hidden message

2. Deterministic Repeatability

The exact same experiment is encoded again.

Expected result:

Same inputs = same encoded output

3. Wrong-Seed Negative Control

The toolkit intentionally attempts to recover the message using an incorrect seed.

Expected result:

Wrong seed should not recover the original message

These checks verify implementation behavior. They do not prove cryptographic security.

500-Experiment Results

The first full Phase 2 run produced:

Cover files tested:               100
Runs per cover:                     5
Total experiments:                500

Correct-seed decode passes:   500 / 500
Repeatability passes:         500 / 500
Wrong-seed rejection passes:  500 / 500

Mean cover modification:          2.204685%
Median cover modification:        2.176294%
Mean eligible-position change:   27.978514%
Mean capacity utilization:       55.948578%
Mean changed words:            1,404.080000
Mean runtime per experiment:     12.305642 ms

The saved results are stored under:

results/
├── phase2_500_experiments.csv
└── phase2_500_summary.txt

These results show that the current implementation behaved consistently under the tested conditions.

They do not establish:

cryptographic security

resistance to steganalysis

natural-language stealth

cross-platform deterministic compatibility

safe real-world communication

Current Research Limitations

Public Astronomical Data

Planetary values are reproducible and should not be treated as secret cryptographic material.

Synthetic Dataset

The current 100-cover Phase 2 dataset is synthetic.

Future testing should include more naturally written controls, including public-domain, openly licensed, or original human-written text.

Visible Letter Substitutions

The current direct substitutions can create spelling errors, letter-confusion patterns, and malformed words.

Those changes are intentional within the CDP concept, but whether they provide useful plausible deniability still needs to be measured.

Limited Carrier Alphabet

Only four letters currently carry data:

b
d
p
q

This limits capacity and concentrates the modifications into a small character set.

Large Cover Requirement

A relatively small payload requires a much larger carrier text because each eligible character stores only one bit.

No Payload Encryption Yet

The current implementation hides and recovers data but does not yet encrypt the payload before embedding.

The celestial seed and dyslexic jitter are not intended to replace encryption.

Cross-Platform Reproducibility

The current implementation was developed and tested using Visual Studio on Windows.

Exact std::shuffle behavior can depend on the C++ standard-library implementation, so cross-platform deterministic compatibility has not yet been proven.

What This Project Is Not

CDP is not:

a replacement for AES or other established cryptography

a proven secure communication system

proven undetectable

proven resistant to professional steganalysis

a production-ready security product

It is an experimental steganography and testing platform.

If confidentiality is added, the plan is to use established encryption rather than create a custom encryption algorithm.

Repository Structure

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
│   ├── Main.cpp
│   └── README.md
│
├── results/
│   ├── phase2_500_experiments.csv
│   └── phase2_500_summary.txt
│
└── dataset/
    └── synthetic-covers/
        └── 100 synthetic cover text files

Generated Phase 2 session folders are intentionally not committed.

The .gitignore excludes toolkit_results/ along with local Visual Studio build files, downloaded CSPICE files, SPICE kernels, and other reproducible output files.

Building the Project

Requirements

Current development environment:

Windows

Visual Studio

C++

NASA/JPL CSPICE Toolkit

DE440 planetary ephemeris kernel

NAIF leap-second kernel

CSPICE Setup

Run:

setup_dependencies.bat

The script downloads the required 64-bit Windows CSPICE toolkit and places the files under:

cspice/include/
cspice/lib/

It also downloads:

kernels/naif0012.tls
kernels/de440.bsp

The Visual Studio project uses relative paths so personal Windows paths are not required.

The project is built using the x64 configuration.

What I Learned

This project has given me hands-on practice with:

C++

file input/output

external C libraries

Visual Studio project configuration

binary representation

deterministic pseudorandom generation

vector manipulation

text processing

data validation

CSV output

automated testing

negative controls

experimental design

debugging

separating implementation success from security claims

One of the biggest lessons has been that something can be technically interesting and repeatable without automatically being secure.

The testing phase also reinforced that measuring weaknesses is just as important as demonstrating that the program works.

Next Steps

Phase 3 - Detection and Steganalysis

The immediate next phase is to build CDP Detector v0.1 and establish a detection baseline.

Planned work includes:

compare original covers against encoded samples

add naturally written control texts

measure false-positive and false-negative rates

test b/d and p/q balance features

measure unusual q frequency

measure eligible-character entropy

detect malformed-word and substitution patterns

compare results across different writing styles

test cross-platform reproducibility

Phase 4 - Authenticated Encryption

After the detection baseline is documented, I plan to add an established authenticated-encryption layer before CDP embedding.

A candidate architecture is:

plaintext
    |
    v
AES-256-GCM or another established authenticated-encryption scheme
    |
    v
ciphertext
    |
    v
CDP embedding

The encryption key will be managed separately from the public celestial inputs.

Phase 5 - Optional Post-Quantum Key Establishment

A later research phase may evaluate standardized post-quantum key establishment for sharing or establishing the symmetric encryption key.

The goal would be to use established implementations and standards rather than designing a custom post-quantum algorithm.

Why I Built It

The main reason I built CDP was curiosity.

I wanted to see whether real planetary movement could be turned into a repeatable value and then used to control a text-steganography experiment.

I also wanted a project that would push me beyond basic classroom C++ and force me to work through external libraries, debugging, testing, data collection, and security design decisions.

Phase 1 established that the encode/decode pipeline works.

Phase 2 gave me a larger testing framework and measurable results across 500 experiments.

The next question is no longer only:

Can the program hide and recover a message?

It is also:

How detectable is the current method, under what conditions does it fail, and what can those failures teach me?

That is the direction I want to continue researching.
