# Requirements Verification Questions

Please answer the following questions to finalize the requirements for the Multi-Profile Alliance Launcher feature.

## Question 1

How should the "archive current config" option be presented to the user?

A) During first-time setup of the multi-profile feature (a one-time migration wizard)

B) As a menu option available at any time (e.g., "[B] Backup current login_w.bin")

C) Both — offer it during first setup AND as a persistent menu option

X) Other (please describe after [Answer]: tag below)

[Answer]: B
Keep in mind that the user can change an archive, as well as create a new archive.
When they select archive, make sure to ask if they want to save to a new slot
or overwrite an old slot. If they want to overwrite, present a list of characters
and what archive slot they relate to. For instance, my accounts are fairly straightforward
currently: 1-4 is archive one, 5-6 is archive two. In this case you would show archive 1
and the list of characters 1-4, then archive 2 and the list of character 5-6. The user
should be able to select either of the options.

## Question 2

When launching multiple accounts sequentially, how should the user signal "ready for next"?

A) Press any key (simplest, matches existing batch behavior with `pause`)

B) Press a specific key (e.g., Enter only)

C) Automatic timer with a configurable delay (e.g., 15 seconds between launches)

D) Both manual key press AND optional auto-timer (user picks in config)

X) Other (please describe after [Answer]: tag below)

[Answer]: B
The user can set a specific key, but the default should be enter

## Question 3

When a profile group swap is needed between sequential launches, should the app warn the user?

A) Yes — display a warning and wait for confirmation before swapping (matches the batch file's `pause` behavior at swap boundaries)

B) Yes — display a warning but swap automatically after a short delay

C) No — swap silently and continue (user already confirmed "next" so just do it)

X) Other (please describe after [Answer]: tag below)

[Answer]: B
The copy takes a few seconds, so we should allow for the file swap to finish before
starting the character. The file swap should happen as soon as the play presses the continue button,
then the countdown happens by itself. During the countdown, the player should not be able
to skip the countdown as they can in the current set up.

## Question 4

Should the POL path be stored in the existing `config.json` or a separate file?

A) In the existing `config.json` (keeps everything in one place)

B) In a separate `paths.json` or similar (separates machine-specific config from account config)

X) Other (please describe after [Answer]: tag below)

[Answer]: A

## Question 5

For the alliance selection UI, what level of preset options do you want?

A) Minimal — just show numbered accounts grouped by party, let user pick individually or type "all"

B) Moderate — presets for "Party 1", "Party 2", "Party 3", "Full Alliance", plus custom queue builder

C) Full — all of (B) plus saved custom presets (e.g., "My farming team" = accounts 1, 3, 7)

X) Other (please describe after [Answer]: tag below)

[Answer]: C
All of these are good options. I want to be able to pick just one character, a set of characters
that I am specifying just this once, a specific party, two specific parties, the full alliance,
and custom groups as well.

## Question 6

Should single-account launch (the current default for 1-account configs) remain unchanged, with the new multi-profile behavior only activating when more than 4 accounts are configured?

A) Yes — if 4 or fewer accounts exist, use the current simple behavior; multi-profile only activates when accounts span multiple profile groups

B) No — always use the new UI even for 1-4 accounts (unified experience)

C) Let the user choose in config which mode to use

X) Other (please describe after [Answer]: tag below)

[Answer]: B
Remember that when they are first setting this up, they will only have 1-4 accounts prior to the
first configuration "backup." If we don't have this set up by default, we won't be able to
set it up at all.

## Question 7

For POL path auto-discovery, which registry approaches should be tried?

A) Check Windows Registry only (standard POL/SE install locations)

B) Check Registry, then scan common install paths as fallback

C) Check Registry, scan common paths, then prompt user if neither works

X) Other (please describe after [Answer]: tag below)

[Answer]: C

## Question: Security Extensions

Should security extension rules be enforced for this project?

A) Yes — enforce all SECURITY rules as blocking constraints (recommended for production-grade applications)

B) No — skip all SECURITY rules (suitable for PoCs, prototypes, and experimental projects)

X) Other (please describe after [Answer]: tag below)

[Answer]: A
I want to make absolutely sure that there is no chance of the software being used to
gain access to the player's passwords and multi-factor auth key.

## Question: Property-Based Testing Extension

Should property-based testing (PBT) rules be enforced for this project?

A) Yes — enforce all PBT rules as blocking constraints (recommended for projects with business logic, data transformations, serialization, or stateful components)

B) Partial — enforce PBT rules only for pure functions and serialization round-trips (suitable for projects with limited algorithmic complexity)

C) No — skip all PBT rules (suitable for simple CRUD applications, UI-only projects, or thin integration layers with no significant business logic)

X) Other (please describe after [Answer]: tag below)

[Answer]: C
