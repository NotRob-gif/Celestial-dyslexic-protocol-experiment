CDP Testing & Detection Toolkit

Overview

This folder contains the Phase 2 testing toolkit for the Celestial-Dyslexic Protocol (CDP) experiment.

Phase 1 focused on proving that the basic CDP encode/decode process could work. Phase 2 focuses on measuring how the implementation behaves across many different test conditions and creating data that can later be used for steganalysis and detector development.

The toolkit is an experimental research tool. A successful test means the implementation behaved as expected under the tested conditions. It does not prove cryptographic security, secrecy, or undetectability.

What the Toolkit Tests

The toolkit automatically varies:

cover text

hidden message

payload length

message number

UTC date and time

planet pair

calculated planetary distances

derived celestial seed

Each experiment performs three main validation checks:

Correct-seed recovery
The encoded message is decoded using the correct seed and compared with the original hidden message.

Deterministic repeatability
The same experiment is encoded again to verify that identical inputs produce identical output.

Wrong-seed negative control
The encoded text is decoded using a deliberately incorrect seed to verify that the original message is not recovered.

Dataset

The preferred repository layout is:

dataset/
└── synthetic-covers/
    └── 100 synthetic cover .txt files

For backward compatibility, the toolkit also accepts the older local layout:

covers/
└── 100 synthetic cover .txt files

The toolkit recursively discovers every .txt file in the selected cover folder.

At least 100 cover files are required.

The current dataset is synthetic. It is useful for controlled implementation testing, but it should not be treated as evidence of performance on natural human writing.

Required Runtime Files

The folder you provide to the toolkit must also contain:

kernels/
├── naif0012.tls
└── de440.bsp

These files are used by NASA/JPL CSPICE for the astronomical calculations.

The main repository includes setup scripts that download the required CSPICE package and kernel files.

Building in Visual Studio

Current development environment:

Windows

Visual Studio

x64 build target

C++20

NASA/JPL CSPICE

The source file is:

testing-toolkit/Main.cpp

If creating a separate Visual Studio project for the toolkit:

Create a C++ Console App.

Add testing-toolkit/Main.cpp as the source file.

Set the platform to x64.

Set the C++ language standard to C++20.

Add the CSPICE include directory:

<CDP repository>\cspice\include

Add the CSPICE library directory:

<CDP repository>\cspice\lib

Add these linker dependencies:

cspice.lib
csupport.lib

Build the project.

The CSPICE folders are intentionally not committed to Git and should be created by the repository setup script.

Running the Toolkit

Run the program and enter the root folder of the CDP repository or working project when prompted.

Example:

C:\path\to\Celestial-dyslexic-protocol-experiment

The toolkit expects that folder to contain:

dataset/synthetic-covers/
kernels/naif0012.tls
kernels/de440.bsp

If dataset/synthetic-covers/ is not present, it will also check the legacy:

covers/

location.

The toolkit then asks for:

Runs per cover [default 5, range 1-20]

and:

Master random seed [0 = create a new random session]

Entering 0 creates a new random session.

Entering a previously recorded master seed allows the toolkit to reproduce the same randomized test sequence, subject to the same environment and implementation.

Output

Each run creates a new timestamped folder under:

toolkit_results/

Example:

toolkit_results/
└── session_YYYYMMDD_HHMMSS_<seed>/
    ├── experiments.csv
    ├── covers.csv
    ├── session_summary.txt
    ├── messages/
    └── encoded/

experiments.csv

Contains one row per experiment, including:

cover file

inferred category

message length

payload bits

message number

UTC time

planet pair

planetary distances

celestial seed

wrong seed

cover size

eligible positions

capacity use

changed characters

changed words

b → d

d → b

p → q

q → p

original and encoded letter counts

balance metrics

eligible-character entropy

correct-seed result

deterministic-repeat result

wrong-seed result

runtime

generated message filename

encoded filename

covers.csv

Contains baseline statistics for the discovered cover files.

session_summary.txt

Contains the high-level results for the full test session.

messages/

Contains the generated hidden messages used in the experiments.

encoded/

Contains the generated encoded cover texts.

The entire toolkit_results/ folder is ignored by Git because the outputs can be regenerated.

Verified 500-Experiment Run

The first major Phase 2 run used:

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

A second 500-experiment verification run was also completed successfully after updating the toolkit to support the GitHub dataset path.

The public repository keeps the first 500-run research dataset under:

results/
├── phase2_500_experiments.csv
└── phase2_500_summary.txt

Current Limitations

Synthetic Cover Text

The current 100-cover dataset is synthetic. Future research should include naturally written control samples.

Visible Word Corruption

Because the current CDP implementation directly substitutes:

b ↔ d
p ↔ q

it can produce malformed words.

That may make the current method detectable through relatively simple linguistic or statistical analysis.

Public Celestial Inputs

Planetary positions are public information.

The celestial seed is therefore treated as a deterministic experimental input, not as a secret cryptographic key.

No Payload Encryption Yet

The current toolkit tests steganographic placement and recovery.

The hidden payload is not yet protected by a standard authenticated-encryption layer.

Planned Detection Work

The next research stage is to build CDP Detector v0.1.

Initial detector features may include:

b/d balance

p/q balance

unusual q frequency

eligible-character entropy

substitution-pattern frequency

malformed-word rate

misspelling rate

comparison of original and encoded character distributions

The detector should eventually be evaluated using both encoded samples and naturally written control texts so false-positive and false-negative rates can be measured.

Planned Security Layer

A future version may add established authenticated encryption before CDP embedding.

A possible architecture is:

plaintext
    ↓
authenticated encryption
    ↓
ciphertext
    ↓
CDP embedding
    ↓
carrier text

The intended separation of responsibilities is:

Encryption
= protects the contents of the message

CDP
= experiments with concealing or obscuring where the payload is carried

The project should use established cryptography rather than a custom encryption algorithm.

A future implementation could evaluate a standard authenticated-encryption mode such as AES-256-GCM. Key exchange or post-quantum key establishment can be studied separately after the authenticated-encryption layer is working correctly.

This planned encryption work is not yet implemented and should not be described as a current security property of CDP.

Research Goal

The main Phase 2 question is no longer only whether the program can hide and recover a message.

The larger question is:

Can linguistic steganography based on intentionally introduced character substitutions be reliably distinguished from normal English text using statistical and linguistic analysis while maintaining an acceptably low false-positive rate?

The testing toolkit is intended to provide the data needed to investigate that question.
