# Celestial-Dyslexic Protocol Experiment

## About This Project

This project started as an idea I had while learning more about cybersecurity, steganography, and C++. I wanted to see if I could take something completely unrelated to normal encryption, like planetary distance data, and use it as part of a repeatable way to hide information inside ordinary-looking text.

I am still a student, so I did not start this project expecting to create a new encryption standard or something that is automatically secure. My main goal was to see if the idea could actually work, get more practice coding in C++, learn how to use an external scientific library, and build something that I could test instead of only talking about it on paper.

The result is a working proof-of-concept that uses NASA/JPL SPICE data to calculate planetary distances, derives a deterministic seed from those distances, and uses that seed to control where hidden data is placed inside a larger piece of text.

The project currently focuses on text-based steganography using controlled letter substitutions and a seed-based "dyslexic jitter" system.

## Quick Start

The repository does **not** include the third-party CSPICE toolkit or the large SPICE kernel files. I added a setup script so the project can prepare those dependencies without hard-coded paths.

On Windows:

1. Extract or clone the repository.
2. Double-click `setup_dependencies.bat`.
3. Wait for the CSPICE toolkit and SPICE kernels to finish downloading. `de440.bsp` is about 114 MB.
4. Open `CDP-Experiment.slnx` in Visual Studio.
5. Build the **x64** configuration.
6. Run the program.

The setup script downloads the official NAIF/JPL Windows CSPICE package plus `naif0012.tls` and `de440.bsp`, then places them in the relative folders already used by the Visual Studio project. Those downloaded folders stay ignored by Git.

If Visual Studio asks to retarget the project to an installed C++ toolset, use the toolset installed with your Visual Studio C++ workload.

---

## What the Experiment Does

The current version of the program follows this general process:

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
encoded_cover.txt is created
        ↓
The same seed reproduces the same position order
        ↓
The hidden message is recovered
```

The important part is that the sender and receiver can independently generate the same seed as long as they use the same agreed-upon inputs.

---

## Why I Used NASA/JPL CSPICE

I wanted the planetary calculations to use real astronomical data instead of made-up numbers.

I downloaded the C version of the NASA/JPL SPICE Toolkit (CSPICE) and linked it to a Visual Studio C++ project.

The two main kernel files used in the project are:

- `naif0012.tls` — leap-second information
- `de440.bsp` — planetary ephemeris data

The program loads these kernels and uses CSPICE to calculate the position of a target planet relative to Earth for a specific date and time.

For example, the program currently uses:

```text
MARS BARYCENTER
JUPITER BARYCENTER
```

CSPICE first returns the X, Y, and Z position of each body relative to Earth. The program then uses `vnorm_c()` to turn that 3D position into one straight-line distance in kilometers.

During one of my tests, the program produced approximately:

```text
Earth to Mars:
266,934,430.70 km

Earth to Jupiter:
913,875,674.78 km
```

This gave me real, repeatable astronomical values to use in the experiment.

---

## Building the Celestial Seed

The program currently uses the relationship between two planetary distances to create a starting value.

For the current test, I used the ratio:

```text
Mars distance / Jupiter distance
```

The program then:

1. Calculates the ratio between the two distances.
2. Multiplies the ratio by `1,000,000` so more of the decimal information can be preserved as a usable number.
3. Multiplies that value by the primary planet distance.
4. Reduces the extremely large result into a manageable range using a modulus.
5. XORs the result with `messageNum`.

The last step is important because changing `messageNum` changes the final seed even if the planets and date stay the same.

Example:

```text
same planets
same date
same cover text
same secret message

messageNum = 1
        ↓
Seed A

messageNum = 2
        ↓
Seed B
```

This lets the program generate a different embedding pattern for different messages.

### Important Note

This planetary value is **not a secret key**.

Planetary positions are public information and can be reproduced by someone else using the same ephemeris data.

I am treating this part as an experimental deterministic input / obfuscation mechanism, not as proven cryptographic security.

---

## The Text Steganography Part

Once the program creates the celestial seed, it uses that seed to control where hidden information is placed in a larger cover text.

The current experiment uses these letter pairs:

```text
b ↔ d
p ↔ q
```

The basic bit mapping is:

```text
b = 0
p = 0

d = 1
q = 1
```

The program scans the cover text and records every location containing one of those four letters.

For example:

```text
position 14  -> b
position 33  -> d
position 51  -> p
position 89  -> q
...
```

Those positions are the possible locations where hidden bits can be stored.

---

## Dyslexic Jitter

I did not want the program to always use the first available letter, then the second available letter, then the third, etc.

That would make the embedding pattern much more predictable.

Instead, the program uses the celestial seed with C++'s `std::mt19937_64` pseudorandom number generator.

It then runs:

```cpp
std::shuffle(...)
```

on the list of eligible positions.

This produces a deterministic shuffled order.

That means:

```text
Same seed
    ↓
Same shuffled positions

Different seed
    ↓
Different shuffled positions
```

I refer to this position-selection behavior as **dyslexic jitter**.

The actual hidden message is still encoded using the `b/d` and `p/q` substitutions, but the seed determines which eligible locations are used.

---

## Converting the Secret Message Into Bits

The secret message is stored in `secret.txt`.

The program reads the entire file, checks that it is between 250 and 500 characters, and converts it into binary.

Every normal text character requires 8 bits.

The program also adds a 32-bit message-length header before the payload.

So the total number of required carrier positions is:

```text
32-bit header
+
(message characters × 8)
```

For the message I tested:

```text
Message characters: 291
Bits required:       2360
```

---

## Cover Text Capacity

The current version can only hide one bit in each eligible `b`, `d`, `p`, or `q` position.

Because of that, the cover text has to be much larger than the hidden message.

For my test cover:

```text
Cover length:        90,346 characters
Eligible positions:  12,679
```

The 291-character payload required:

```text
2,360 positions
```

So the program reported:

```text
Capacity result: PASS
```

This also means the program used only part of the available carrier positions.

That capacity limitation is one of the biggest weaknesses of the current design and something I want to test more in future versions.

---

## Encoding Process

The encoder currently works like this:

1. Read `secret.txt`.
2. Confirm the message is between 250 and 500 characters.
3. Convert the message length into a 32-bit header.
4. Convert the message itself into binary.
5. Read `cover.txt`.
6. Find every `b`, `d`, `p`, and `q` position.
7. Verify that the cover has enough eligible positions.
8. Create the celestial seed.
9. Shuffle the eligible positions using the seed.
10. Write each hidden bit into the selected positions.
11. Save the modified text as:

```text
encoded_cover.txt
```

---

## Decoding Process

The decoder performs the same setup again.

It:

1. Recreates the same celestial seed.
2. Finds all eligible positions in the encoded cover.
3. Shuffles the positions using the same seed.
4. Reads the first 32 hidden bits.
5. Reconstructs the message length.
6. Reads the correct number of payload bits.
7. Converts the recovered bits back into text.
8. Saves the recovered message as:

```text
recovered.txt
```

The program then compares `recovered.txt` with the original secret message.

My test result was:

```text
SUCCESS
Recovered message matches secret.txt exactly.
```

This was one of the main milestones I wanted to reach because it showed that the process was deterministic and reversible.

---

## Jitter Test

After the encoder and decoder worked, I wanted to test whether changing `messageNum` actually changed the embedding pattern.

I ran the exact same experiment twice.

Everything stayed the same:

```text
Same secret message
Same cover text
Same planets
Same date
```

The only thing I changed was:

```text
messageNum = 1
```

and then:

```text
messageNum = 2
```

I saved both encoded covers:

```text
encoded_cover_msg1.txt
encoded_cover_msg2.txt
```

Then I added a comparison function that checked both files character-by-character.

The results were:

```text
Characters compared: 90,346
Different positions:  2,115
Difference percent:   2.3410%

RESULT: PASS
```

So changing only `messageNum` caused 2,115 positions in the final encoded cover to differ.

Both versions still decoded back to the exact same original 291-character secret message.

That was useful because it showed that changing the seed actually changed the visible embedding pattern instead of only changing a number printed by the program.

---

## Current Test Results

Current proof-of-concept results:

```text
Secret message size:       291 characters
Payload bits required:     2,360
Cover characters:          90,346
Eligible carrier positions:12,679
Encoding capacity:         PASS
Recovered message:         Exact match
Jitter comparison:         PASS
Different positions:       2,115
Cover difference:          2.3410%
```

I also compared each saved encoded cover against the original cover file. In the current test, message #1 changed 1,159 characters and message #2 changed 1,166 characters. The larger 2,115-position difference above is the direct comparison between the two encoded outputs, not the number of characters changed from the original cover.

---

## What I Learned Building This

This project ended up teaching me more than I expected.

### Using an External C Library in C++

CSPICE is written in C, so I had to learn how to link it into a C++ project.

That included:

- configuring Visual Studio include directories
- configuring library directories
- linking `cspice.lib`
- using `extern "C"`
- troubleshooting working directories
- troubleshooting missing kernel files
- learning how CSPICE kernel files are loaded

A lot of the early progress was honestly just figuring out why the program could not find a file or why Visual Studio was looking in a different directory than I expected.

That troubleshooting ended up being useful practice by itself.

### Real Data Is Better Than Placeholder Values

Originally, the planetary portion was only an idea on paper.

Using CSPICE forced me to work with actual ephemeris data and made the project much more concrete.

Instead of saying:

```text
"assume Mars has this value"
```

the program now actually asks NASA/JPL data where Mars and Jupiter are relative to Earth at a specific time.

### Deterministic Does Not Mean Secret

One of the biggest things I learned is that something can be complicated, repeatable, and interesting without automatically being cryptographically secure.

Planetary data is public.

If someone knows the date, planets, calculation method, and message number, they may be able to reproduce the same values.

That is why I currently describe the celestial portion as experimental obfuscation / deterministic input rather than encryption.

### Capacity Matters

I also learned how quickly payload size becomes a problem in text steganography.

A 291-character message required 2,360 eligible carrier positions.

That means the cover needs to be significantly larger than the hidden message.

This is one reason compression and additional carrier techniques may be worth testing later.

### Testing Is More Useful Than Assuming

At first, it was easy to say things like:

```text
"changing the seed should change the pattern"
```

But it was much better to actually save two outputs and compare them.

The comparison showed:

```text
2,115 different positions
```

which gave me an actual measurable result.

That changed the way I started thinking about the project. I want future changes to be based on measurements instead of assumptions.

---

## What This Project Is Not

This project is **not** a replacement for real cryptography.

It is not currently:

- proven secure
- proven undetectable
- resistant to professional steganalysis
- a replacement for AES or other established encryption
- a finished secure communication system

The current goal is experimentation and learning.

The celestial seed controls the embedding pattern, but it does not provide the same protection as a real secret cryptographic key.

If this project eventually includes actual message confidentiality, I plan to use established encryption rather than inventing my own encryption algorithm.

A future version could look more like:

```text
plaintext
   ↓
compression
   ↓
standard encryption
   ↓
encrypted payload
   ↓
CDP steganographic embedding
```

In that design:

```text
Encryption
= protects what the message says

CDP
= attempts to hide the presence/location of the payload
```

---

## Repository Files

The public repository is intentionally small and keeps the experiment files together so it is easy to follow:

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
└── encoded_cover_msg2.txt
```

`encoded_cover.txt` and `recovered.txt` are generated when the program runs, so I do not keep them in source control. The CSPICE toolkit and SPICE kernels are also downloaded separately and ignored by Git because they are external dependencies, not code I wrote.

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

I do not commit CSPICE or the SPICE kernels to this repository because they are third-party dependencies and `de440.bsp` is larger than GitHub's normal single-file limit.

For Windows, the easiest setup is to run:

```text
setup_dependencies.bat
```

That script downloads the official 64-bit Windows CSPICE toolkit from NAIF/JPL and places it at:

```text
cspice/include/SpiceUsr.h
cspice/lib/cspice.lib
cspice/lib/csupport.lib
```

It also downloads the two kernel files used by this experiment:

```text
kernels/naif0012.tls
kernels/de440.bsp
```

The included Visual Studio project uses those relative paths, which keeps personal Windows paths out of the project. The `cspice/` and `kernels/` folders are ignored by Git.

The C++ source includes CSPICE using:

```cpp
extern "C"
{
#include "SpiceUsr.h"
}
```

I use the **x64** build configuration in Visual Studio. The repository intentionally does not provide a Win32/x86 configuration because the automated setup uses the 64-bit Windows CSPICE package.

Official dependency sources used by the setup script:

- NAIF/JPL CSPICE Windows 64-bit package: `https://naif.jpl.nasa.gov/pub/naif/toolkit/C/PC_Windows_VisualC_64bit/packages/cspice.zip`
- NAIF leap-second kernel: `https://naif.jpl.nasa.gov/pub/naif/generic_kernels/lsk/naif0012.tls`
- JPL DE440 planetary ephemeris: `https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de440.bsp`


---

## Current Limitations

The biggest limitations I see right now are:

### 1. Public Astronomical Data

The celestial values can be reproduced.

They should not be treated as a secret.

### 2. Small Carrier Alphabet

Only four letters currently carry information:

```text
b
d
p
q
```

This severely limits capacity.

### 3. Large Cover Requirement

A relatively small hidden message requires a large cover.

### 4. Detectability Has Not Been Properly Tested

The current project proves that the message can be hidden and recovered.

It does **not** prove that an analyst or machine-learning detector would fail to detect the modified text.

That is one of the main questions I still want to study.

### 5. Synthetic Cover Text

My current large cover file was created mainly to test capacity and program behavior.

It should not be used as evidence that the method performs the same way on natural writing.

A better future test would use multiple realistic cover sources and compare results.

### 6. Current Reproducibility Scope

The current prototype was built and tested with Visual Studio on Windows. The seed itself is deterministic, but `std::shuffle` behavior can depend on the C++ standard-library implementation. I have not yet tested whether an encoded file created with a different compiler or standard library will reproduce the exact same shuffled order.

---

## Next Steps

The next things I want to work on are:

- automatically count each type of substitution:
  - `b → d`
  - `d → b`
  - `p → q`
  - `q → p`
- calculate the percentage of the original cover actually modified
- test multiple `messageNum` values automatically
- test different dates
- test different planet pairs
- test product, ratio, and difference calculations
- export experiment results to CSV
- test different payload sizes
- test realistic cover text
- add compression
- eventually add standard encryption before embedding
- create a simple detector to measure whether the modified text can be distinguished from normal text

---

## Why I Built It

The main reason I built this was curiosity.

I wondered if I could take something like real planetary movement, turn it into a repeatable value, and use that value to control a text-steganography experiment.

I also wanted something that would force me to practice more than basic classroom C++.

This project made me work with:

- C++
- file input/output
- external libraries
- Visual Studio configuration
- binary representation
- deterministic pseudorandom generation
- vector manipulation
- text processing
- data validation
- reproducible testing
- debugging
- basic experimental design

There are still a lot of weaknesses and unanswered questions, but getting the full pipeline working was the first thing I wanted to prove.

At this point, the project can take a 250–500 character message, hide it inside a much larger text using a celestial-derived jitter pattern, recover the original message exactly, and demonstrate that changing the message number changes the embedding pattern.

That is enough for me to consider the first proof-of-concept successful, and now I can focus more on measuring where it works, where it fails, and whether the idea has any real research value.
