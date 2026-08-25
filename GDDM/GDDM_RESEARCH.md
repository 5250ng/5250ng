# IBM i GDDM graphics over 5250: research and 5250ng implementation notes

Last updated: 2026-08-25

## Implementation status

The protocol layer - error classification `G1`..`G5`, the fatal/recoverable
split, the Graphic Aid key codes, Read Status, the indexed colour-index plane,
`B3` OR/XOR/Replace and a retroactive `B4` - landed on `main` in PR #182 as
`ErrorCode`, `Completion`, `statusBytes()`, `RasterFunction` and `writePel()`.

This branch adds what draws: `A5` Fill Polygon, `A1` Draw Scanline, `A4`/`B5`
markers, `B2` Set Style Offset, the plane/text layering fix, block diagnostics
and a decoder fuzzer, and the printer orders behind screen copy. Before it,
`main` answered `G2` "unsupported drawing order" to `A1`, `A4`, `A5` and `A6`,
which fails any picture containing a fill, an image, mode-2 text or a marker.

## Executive conclusion

IBM i GDDM is an optional IBM i operating-system component (5770-SS1 option 14) that turns high-level drawing calls such as `GSLINE`, `GSCHAR`, and `GSAREA` into device-specific output. For a classic graphics display, it targets the IBM 5292 Model 2 protocol: special graphics blocks embedded in otherwise normal 5250 Write-to-Display traffic. This is not GOCA-over-WDSF, not an image/fax structured field, and not ordinary character-cell line drawing.

5250ng defaults to `IBM-3179-2`. On the test IBM i, GDDM recognizes that as a non-graphics terminal and silently substitutes a dummy `5292M2` device: the example program compiles and executes, but no graphics block is sent. Supporting GDDM therefore requires both:

1. negotiating or otherwise exposing a graphics-capable 5292-style workstation to IBM i; and
2. parsing, pacing, retaining, compositing, and rendering the 5292 graphics data stream.

Both are now resolved in principle. Requesting `IBM-5292-2` is sufficient for (1) — IBM i accepts the terminal type directly, with no `GR5250`-style out-of-band step — and a live picture confirms (2) end to end apart from area fill. See [filled areas](#filled-areas-how-gddm-lowers-gsarea-to-the-wire).

IBM i Access Client Solutions (ACS) does **not** support the old IBM i 5250/5292 GDDM display graphics feature. ACS has normal 5250 display and printer emulation, but IBM documents its only display device requests as `3179-2`, `3477-FC`, and `5555-C01`; none is the 5292 Model 2 graphics workstation. ACS is based on Host On-Demand technology, whose compatibility material distinguishes host graphics support from ordinary 5250 emulation and does not provide 5250 GDDM graphics. This matches the live IBM i result: a `3179-2` session gets dummy-device GDDM behavior. Do not confuse ACS printer emulation, AFP/image support, or mainframe 3270 GDDM support with this IBM i 5292 protocol.

## Primary references

- IBM, *AS/400 Programming: GDDM Programming Guide*, SC41-0536-00: <https://public.dhe.ibm.com/systems/power/docs/systemi/v5r4/en_US/sc410536.pdf>
- IBM, *AS/400 Programming: GDDM Reference*, SC41-3718-00: <https://public.dhe.ibm.com/systems/power/docs/systemi/v5r4/en_US/sc413718.pdf>
- IBM, *5250 Information Display System Functions Reference Manual*, SA21-9247-6: <https://bitsavers.trailing-edge.com/pdf/ibm/5250_5251/SA21-9247-6_IBM_5250_Information_Display_System_Functions_Reference_Manual_198703.pdf>
- IBM i current documentation, graphics calls: <https://www.ibm.com/docs/en/i/7.4.0?topic=statement-i-graphics-support>
- IBM i current documentation, GDDM manuals remain listed: <https://www.ibm.com/docs/en/i/7.5.0?topic=documentation-pdf-files-manuals>
- IBM i licensed-program option list (5770-SS1 option 14 is GDDM): <https://www.ibm.com/docs/ssw_ibm_i_74/rzahc/lptimz.htm>
- IBM ACS supported TN5250 device types: <https://www.ibm.com/support/pages/device-types-requested-ibm-i-access-client-solutions>
- IBM ACS overview (5250 display and printer emulation are Host-on-Demand-based): <https://www.ibm.com/support/pages/ibm-i-access-client-solutions>
- IBM Personal Communications graphics protocols (useful contrast; this product explicitly supports host GDDM): <https://www.ibm.com/docs/en/personal-communications/14.0.0?topic=functions-graphics-protocols>
- IBM Host On-Demand printing behavior: <https://www.ibm.com/docs/en/host-on-demand/16.0.0?topic=printing-from-host-workstation-5250>
- IBM GDDM output to alternate devices: <https://www.ibm.com/docs/en/gddm?topic=programs-sending-output-device-other-than-invoking-device>

Local extracted copies used for this research are in [`research/`](research/):

- `sc410536.pdf` and `sc410536.txt`
- `sc413718.pdf` and `sc413718.txt`
- `sa2192476.pdf` and `sa2192476.txt`

The first two are IBM i GDDM manuals. Do not substitute the similarly named z/OS GDDM V3.2 manuals when implementing the IBM i 5250 path.

## Verified target sandbox

Target system: `RSRCH09`, IBM i `V7R5M0`.

The authorized test was performed through the TN5250 MCP. No credential is stored in this file.

Verified state:

- Product library `QGDDM` exists and contains 684 objects, including individual routine programs such as `ASREAD`, `GSLINE`, and the GDDM dispatcher.
- Sandbox library `GDDMTEST` was created as `TYPE(*TEST)`.
- Source file `GDDMTEST/QRPGSRC` was created with record length 112.
- Source member `GDDMDEMO` was added.
- Program `GDDMTEST/GDDMDEMO` was compiled successfully with `CRTRPGPGM`; compiler result was severity 00.
- Calling `GDDMTEST/GDDMDEMO` completes successfully in the MCP session.
- The job log stated: `Dummy device support 5292M2 provided for DSOPEN request.` This is the key evidence that the current `IBM-3179-2` negotiation does not activate host-to-terminal graphics.

Verified additionally on 2026-08-25:

- IBM i accepts `IBM-5292-2` as a TN5250 terminal type directly. No `GR5250`-style out-of-band capability exchange was needed, and no dummy-device substitution occurred.
- With that device type, the host sends real 5292 graphics blocks and 5250ng renders them. See [filled areas](#filled-areas-how-gddm-lowers-gsarea-to-the-wire).
- Source member `GDDMSHAF` was added to `GDDMTEST/QRPGSRC` and compiled with `CRTRPGPGM` at severity 00. Source was uploaded by FTP to `GDDMTEST/QRPGSRC.GDDMSHAF`, which is far more practical than typing it through SEU.
- The capture was started only after sign-on, so no credential appears in `captures/`.

Commands used:

```cl
CRTLIB LIB(GDDMTEST) TYPE(*TEST) TEXT('GDDM sandbox')
CRTSRCPF FILE(GDDMTEST/QRPGSRC) RCDLEN(112) TEXT('GDDM samples')
CRTRPGPGM PGM(GDDMTEST/GDDMDEMO) \
           SRCFILE(GDDMTEST/QRPGSRC) REPLACE(*YES)
CALL GDDMTEST/GDDMDEMO
```

Current fixed-format RPG/400 source:

```text
     IPARAM       DS
     I                                    B   1   40ATTYPE
     I                                    B   5   80ATMOD
     I                                    B   9  120COUNT
     C                     CALL 'GDDM'
     C                     PARM 'FSINIT ' RTN     8
     C                     CALL 'GDDM'
     C                     PARM 'GSLINE ' RTN
     C                     PARM 90        X       51
     C                     PARM 90        Y       51
     C                     CALL 'GDDM'
     C                     PARM 'ASREAD ' RTN
     C                     PARM 0         ATTYPE
     C                     PARM           ATMOD
     C                     PARM           COUNT
     C                     CALL 'GDDM'
     C                     PARM 'FSTERM ' RTN
     C                     SETON                     LR
     C                     RETRN
```

`ATTYPE`, `ATMOD`, and `COUNT` must be 4-byte binary fields. Defining them as packed decimal makes the special RPG `CALL 'GDDM'` interface reject ASREAD parameters with CPF8680/data-type diagnostics.

On a true graphics device, the default current position is `(0,0)`, `GSLINE` draws to `(90,90)` in the default GDDM world coordinates, and `ASREAD` forces the output and waits for an interaction. On the current MCP session it targets the dummy device and returns without visible graphics.

## GDDM application model

A basic program has five phases:

1. `FSINIT` initializes the graphics environment. It must be the first GDDM call.
2. Set drawing attributes such as color (`GSCOL`), line width (`GSLW`), character mode/font, fill mode, and mixing function.
3. Build retained graphics primitives with calls such as `GSMOVE`, `GSLINE`, `GSPLNE`, `GSCHAR`, `GSAREA`/`GSENDA`, `GSIMG`, and `GSIMGS`.
4. `FSFRCE` sends a page/update; `ASREAD` sends it and waits for input. On a printer, either operation causes page eject.
5. `FSTERM` frees the graphics environment and must be last.

Defaults and coordinate systems:

- The GDDM default graphics window is normally 0 through 100 in X and Y, with origin at the lower left.
- GDDM maps that device-independent space into the device viewport.
- The 5292 device itself is a 480 by 288 PEL surface: X `0..479`, Y `0..287`, again with the origin at the lower left.
- A renderer with a top-left UI origin maps device Y as `screenY = 287 - gddmY`.
- Graphics and ordinary alphanumeric display-file content are separate planes. They may be shown together when the display file uses the DDS `ALWGPH` keyword.
- Entering graphics display mode reduces line spacing and shows a blue `G` indicator on the original 5292. Graphics mode locks the normal keyboard while a block is being processed.
- GDDM retains pages, segments, attributes, symbols, and primitives on the host side; the 5292 retains a graphics bitmap/display buffer on the terminal side.

The IBM guide's canonical envelope example uses:

```text
FSINIT
GSLW 2
GSCOL 5
GSMOVE/GSLINE ...                  outline
GSCOL 2
GSAREA 1 + polygon + GSENDA        filled stamp
GSCOL 4
GSCHAR ...                         address
ASREAD
FSTERM
```

## 5250 graphics block framing

The 5292 stream is carried by a normal 5250 Write-to-Display command. It is distinguished from alphanumeric data by the first display-data byte being `FF` (Begin Graphics). In 5250ng's current layers, detection belongs before `TN5250StreamRenderer` treats bytes `>= 0x40` as EBCDIC text.

Conceptual host record:

```text
GDS header ... ESC(04) WTD(11) CC1 CC2  FF <graphics orders/data> ...
                                         ^ first WTD data byte
```

Rules:

- `FF` as the first graphics byte enters graphics mode.
- `FF FF` at the beginning is System Graphics Reset.
- A single block ends with `95` (End Graphics).
- `90` ends the current block but keeps graphics mode active for another nonspanned block.
- `91` is More Data to Come: it ends a block in the middle of a variable-length order.
- `92` is End of Data: it terminates the variable data belonging to the preceding order.
- `93` turns graphics display on.
- `94` turns graphics display off without converting the following bytes to text.
- `95` ends graphics mode.
- `96` suppresses the normal pacing response for the current block. It must be honored carefully.
- Graphics and alphanumeric bytes cannot be mixed in one Write block.
- While graphics mode remains active, subsequent Write blocks are graphics even when they do not start with `FF`; mode ends on `95`, System Graphics Reset, or a nonrecoverable graphics error.

There is an apparent limit difference between IBM manuals:

- the physical 5292 Functions Reference specifies 11 to 256 bytes per graphics write block;
- the IBM i GDDM guide's UDDS discussion specifies greater than 11 and at most 1920 bytes.

This is probably a controller/IBM i layering difference. Do not reject a TN5250 block merely because it exceeds 256 bytes. Preserve order state across records and verify the actual IBM i TCP output after enabling 5292 negotiation. The TN5250 GDS record length remains the outer framing authority.

## Graphics byte classes and order table

Every byte inside graphics mode has a class in its top two bits:

| Top bits | Meaning |
|---|---|
| `00` | invalid graphic byte |
| `01` | graphics data; low six bits are payload |
| `10` | graphics order |
| `11` | I/O feature order (printer or IEEE attachment) |

Important orders:

| Byte | Order | Following data |
|---:|---|---|
| `B0` | Set Color | 1 graphics-data byte, index 0..7 |
| `B1` | Set Style | 4 data bytes: visible, gap, visible, gap lengths |
| `B2` | Set Style Offset | 1 data byte |
| `B3` | Set Function | 1 data byte: OR, XOR, or Replace |
| `B4` | Set Color Table | variable index/RGB data, terminated by `92` |
| `B5` | Set Marker | 1 data byte |
| `B6` | Set Line Weight | 1 data byte, single or double |
| `B7` | Set Fill Mode | 2 data bytes |
| `80` | Read Status | 2 data bytes: A/N buffer offset |
| `A0` | Draw Polyline | variable coordinate pairs, terminated by `92` |
| `A1` | Draw Scanline | X/Y followed by 1-bit PEL pattern, terminated by `92` |
| `A3` | Write Background | 1 color-index data byte |
| `A4` | Write Polymarker | variable coordinates, terminated by `92` |
| `A5` | Fill Polygon | variable coordinates, terminated by `92` |
| `A6` | Define Shield Area | variable coordinates, terminated by `92` |
| `C0` | Printer Data Follows | variable, terminated by `92` |
| `C1` | Screen Copy | no data; original 5292 printer attachment/RPQ path |
| `C2` | Load printer A/N color-mix table | variable |
| `C3` | Load printer graphics color-mix table | variable |
| `C4` | Set printer timeout | 1 data byte |

**Correction, verified on the wire 2026-08-25.** The Functions Reference tables print the Set orders as `80`..`87` *and* Read Status as `80`, which cannot both be true. A live capture from RSRCH09 settles it: IBM i emits the Set orders in the `B0`..`B7` range, and `80` is Read Status alone. `B0`, `B1`, `B2`, `B3`, `B4`, `B6`, and `B7` were all observed with exactly the data-byte counts the manual gives for `80`..`87`; no byte in `81`..`87` ever appeared. The manual's Set-order table is off by `0x30`.
The manual contradicts itself and its bit diagrams win: the Set Marker order's
own format diagram spells the order byte out bit by bit as `1 0 1 1 0 1 0 1`,
that is `B5`, not the `85` its summary table prints. See
[`research/marker-shapes-page-2-87.png`](research/marker-shapes-page-2-87.png). Draw orders (`A0`, `A3`, `A5`, ...) and control bytes (`90`..`96`) are *not* shifted and match the manual as printed. The order byte alone is therefore unambiguous, but the parser must still be stateful for variable orders and block spanning.

Default palette:

| Index | RGB intensity (`0..7` each) | Color |
|---:|---|---|
| 0 | 000 | black (not redefinable) |
| 1 | 700 | red |
| 2 | 070 | green |
| 3 | 007 | blue |
| 4 | 707 | pink/magenta |
| 5 | 770 | yellow |
| 6 | 077 | turquoise/cyan |
| 7 | 777 | white |

The current drawing color defaults to index 7. Line style defaults to solid. Function defaults to Replace. Line weight defaults to single. Marker defaults to a solid 5x5 box.

**Do not rely on this default palette when reading a capture.** IBM i GDDM issues a `B4` Set Color Table in its opening block that redefines all seven writable indexes, so the wire palette is not the device default:

| Index | GDDM `B4` value (r,g,b) | Colour | Device default |
|---:|---|---|---|
| 1 | 0,0,7 | blue | red |
| 2 | 7,0,0 | red | green |
| 3 | 7,0,7 | magenta | blue |
| 4 | 0,7,0 | green | pink/magenta |
| 5 | 0,7,7 | cyan | yellow |
| 6 | 7,7,0 | yellow | turquoise/cyan |
| 7 | 7,7,7 | white | white |

This is GDDM aligning the device to its own colour numbering. A renderer that honours `B4` shows correct colours; one that ignores it shows a permuted palette that looks like a colour bug but is not.

## Six-bit graphics-data decoding

Graphics data bytes always have high bits `01`; the useful value is `byte & 0x3F`.

Coordinates are 10-bit values encoded in two graphics-data bytes. The first byte holds the high four coordinate bits in its low nibble and the second holds the low six bits:

```cpp
bool isGraphicsData(uint8_t b) { return (b & 0xC0) == 0x40; }

uint16_t decodeCoord(uint8_t hi, uint8_t lo) {
    // validate (hi & 0xF0) == 0x40 and (lo & 0xC0) == 0x40
    return uint16_t((hi & 0x0F) << 6) | uint16_t(lo & 0x3F);
}

std::array<uint8_t, 2> encodeCoord(uint16_t v) {
    return {uint8_t(0x40 | ((v >> 6) & 0x0F)),
            uint8_t(0x40 | (v & 0x3F))};
}
```

IBM's polyline example encodes `(0,10) -> (100,50) -> (150,25)` as:

```text
A0 40 40 40 4A 41 64 40 72 42 56 40 59 92
```

A useful synthetic single-block fixture is:

```text
FF 93 80 41 A0 40 40 40 4A 41 64 40 72 42 56 40 59 92 95
```

Meaning: Begin Graphics, display on, set color index 1 (red), draw the three-point polyline, end its variable data, and end graphics mode. This is a parser/unit-test fixture; the IBM i host will supply the surrounding WTD/GDS framing.

## Filled areas: how GDDM lowers GSAREA to the wire

Captured live from RSRCH09 on 2026-08-25 with `GDDMTEST/GDDMSHAF`
([`GDDMSHAF.rpg`](GDDMSHAF.rpg)), the `GSAREA` variant of `GDDMSHAP.rpg`.
Artifacts: [`captures/gddm-gsarea.pcap`](captures/gddm-gsarea.pcap),
[`captures/gddm-gsarea.decoded.txt`](captures/gddm-gsarea.decoded.txt),
[`captures/gsarea-live.png`](captures/gsarea-live.png). Decode a capture with
[`tools/decode_gddm_capture.py`](tools/decode_gddm_capture.py).

Yes, filled polygons work, and GDDM uses **two different lowerings** depending
on the shape. This matters a great deal for a renderer: only one of them uses
the device fill order at all.

### 1. Device-side fill: `B7` Set Fill Mode + `A5` Fill Polygon

Used for a single closed figure. Observed three times, always as the pair
`B0` Set Color, `B1` Set Style, `B7` Set Fill Mode, `A5` Fill Polygon:

| Source shape | `GSAREA` | `GSPAT` | `B7` bytes | `B1` style | `A5` points |
|---|---:|---:|---|---|---:|
| square | 1 | 0 | `40 40` | `4F 40 4F 40` solid | 4 |
| triangle | 0 | 0 | `42 40` | `4F 40 4F 40` solid | 3 |
| star (concave, 10 vertices) | 1 | 5 | `42 40` | `41 43 41 43` dashed | 10 |

Reading `B7` byte 2 as the manual's `aaTbbb` layout: value `0` is
`aa=00, bb=00` (vertical reference line, solid boundary plus style fill) and
value `2` is `aa=00, bb=10` (fill interior only). So `GSAREA(0)` — fill with no
outline — maps to `bb=10`, and `GSAREA(1)` maps to `bb=00`. Non-solid
`GSPAT` patterns are expressed by loading a dashed `B1` Set Style before the
fill, not by any pattern-bitmap order; the device draws the interior as styled
lines along the reference line.

Note the star: GDDM sends the raw 10-point outline and lets the *device* work
out the interior, so a renderer needs a real scanline fill and cannot assume
convexity. It must **not** assume the star's core is a hole, either: the
outline is traced tip, inner, tip, inner, which is a simple concave polygon,
not a pentagram, so the core counts as inside under both the even-odd and the
nonzero-winding rule. All three captured polygons are simple, so this capture
does not by itself discriminate between the two rules; even-odd is what the
device documents and is what 5250ng implements.

### 2. Host-side rasterization into `A0` Draw Polyline

Used when the device fill order cannot express the shape. GDDM decomposes the
area on the host and ships plain polylines. Two cases were observed:

- **Multiple closed figures in one bracket (a hole).** The house-with-window
  shape produced no fill order at all — instead one horizontal `A0` polyline
  per device scanline, and on the scanlines crossing the window, *two*
  segments per line with the hole skipped, e.g. `(47,161)-(67,161)` and
  `(105,161)-(124,161)`. The even-odd rule was applied host-side.
- **Hatch patterns.** `GSPAT 3` on the kite produced roughly sixty short `A0`
  strokes plus the outline polyline, rather than a styled `A5`.

**`A6` Define Shield Area was never emitted.** Across the whole picture the
tally was 182 `A0`, 3 `A5`, 3 `B7`, and zero `A6`. IBM i GDDM appears not to
use shield areas at all: it prefers host-side scanline decomposition. A
decoder should still parse `A6` defensively, but it is not on the critical
path for GDDM-generated pictures.

### Block spanning is exercised for real

The picture spanned 13 graphics blocks over 17 GDS records. Only the first
block began with `FF`; every later one relied on graphics mode persisting
because the preceding block ended with `90`, not `95`. Five `A0` orders were
split mid-coordinate by `91` More Data to Come and resumed in the next
block — including one split with an odd 3 data bytes buffered, and one where
the order byte and terminator landed in different blocks. A decoder that does
not carry both graphics mode *and* pending-order data across records will
desynchronize here. Pacing worked: 13 blocks produced 13 `0x3C` responses and
the host never stalled.

Also observed, and not in the manual's picture of a minimal stream: GDDM pads
short blocks with long runs of idempotent control bytes to satisfy the
minimum block length — 18 or 100 repetitions of `93` Graphics Display On, and
runs of `90`. It also emits one `C3` order with 14 data bytes in its opening
block, which the manual lists as a printer colour-mix table load; its purpose
here is unexplained and it appears harmless to ignore.

### Consequence for 5250ng, and the implementation

Before this work `src/ui/rendering/gddm_5292_decoder.cpp` routed `A5` to
`PendingOrder::Ignore` and validated `B7` Set Fill Mode without storing it, so
filled areas were consumed correctly but never painted. The live test
reproduced that exactly: the three `A5` shapes were absent from the rendered
plane while every `A0`-drawn shape appeared correctly.

`A5` is now implemented:

- `B1` Set Style run lengths and `B7` Set Fill Mode are retained in decoder state.
- Interior shading is an even-odd scanline fill sampling at PEL centres, so a vertex landing exactly on a scan line is counted once.
- The style is applied across the fill lines, indexed by a phase that depends on the `aa` reference direction (`x` for vertical, `x - y` and `x + y` for the two diagonals, all in device space) plus the `cccccc` leftward shift. A solid style therefore yields a solid block and a dashed style a hatch, with no separate pattern order involved.
- More than `128` nonhorizontal edges on the closed figure fails the block with device error `G4`.
- `A5` split by `91` More Data to Come resumes through the existing pending-order buffer.

`aa=01` (interior style follows the polygon edge) needs per-edge distance
fields and is approximated by the vertical reference rather than refused; no
captured stream has requested it. `A1`, `A4`, and `A6` remain parsed but not
rendered. `A0` still ignores `B1`, so dashed *polylines* draw solid — the
style is currently consulted only for fills.

Replaying `captures/gddm-gsarea.pcap` through the decoder now reproduces all
six shapes with zero graphics errors and a pacing response for every block;
see [`captures/gsarea-replay-after-fill.png`](captures/gsarea-replay-after-fill.png)
against the pre-implementation [`captures/gsarea-live.png`](captures/gsarea-live.png).

One trap when replaying to the end of a capture: after `ASREAD` returns, GDDM
tears the picture down with an `A3` Write Background of colour 0, which fills
the plane opaque black. A replay that runs past that point correctly ends up
with a blank plane. Stop before the teardown block to see the picture.

## Pacing and keyboard behavior

Pacing is mandatory for reliable host operation.

1. Host sends one graphics Write block.
2. Terminal recognizes graphics mode, saves the block, locks normal input, and processes it.
3. Terminal sends a graphics AID when processing completes.
4. Host must not send the next graphics block before that AID.
5. On the final block, `95` leaves graphics mode and unlocks normal input.

Success is Command-12/24, whose device scan-code pair is documented as `6F 3C`. At the 5250 application level, `3C` is the normal PF12/Command-12 AID value. 5250ng should construct the normal terminal response through its existing AID/GDS response path with AID `0x3C` and no MDT fields. Do **not** copy raw `6F 3C` directly onto the TN5250 socket without first confirming the layer expected by IBM i.

Other documented results:

| Result | AID meaning | Device scan bytes |
|---|---|---|
| success | Cmd-12/24 | `6F 3C` |
| operator terminated graphics | Cmd-11/23 | `6F 38` in the manual table |
| fatal processing error | Cmd-10/22 | `6F 3A` |
| recoverable error, continued | Cmd-9/21 | `6F 39` |
| System Graphics Reset | Cmd-8/20 | documented Cmd-8 response; never suppressed |

The scan-code table has historical command-key aliases and values that do not map one-for-one to 5250ng's modern PF13-PF24 AID constants. Treat `0x3C` (PF12) as the known success AID and validate all other responses with a target capture before implementing error replies.

If order `96` is present, suppress the ordinary completion AID for that block. System Graphics Reset still requires its response.

## Error handling

Device errors:

- `G1`: invalid graphics byte, misplaced MDTC, or data when an order was expected.
- `G2`: undefined/unsupported order.
- `G3`: invalid set-order data.
- `G4`: polygon has more than 128 nonhorizontal fill/shield edges.
- `G5`: marker outside display boundaries; recoverable.
- `P1..P5`: attached-printer errors.
- `E1..E5`: IEEE-488 attachment errors.

For an emulator, parse errors should:

- stop consuming the malformed logical order without allowing its bytes to fall through as EBCDIC;
- retain a diagnostic with record offset and order;
- send the correct error completion AID once validated;
- terminate graphics mode on nonrecoverable errors;
- keep UI and network threads responsive even for large fills or scanlines.

GDDM application errors use IBM i messages `CPGxxxx` and escape messages in the `CPF8600` range. Without a custom `FSEXIT`, severity 40 results in CPF8619. `FSQERR` returns the last structured GDDM error.

## Implementation status: error handling and the indexed plane

Two rounds of work landed after the fill rasterizer. Both are decoder-side; the
`Q5250ScreenWidget` boundary (`setGddmGraphicsPlane(QImage, bool)`) is unchanged.

### Graphic Aid key codes, resolved

The Functions Reference table "Graphic Aid Key Codes" gives Cmd-1 through Cmd-7
as reserved `6F 31`..`6F 37`, Cmd-9 `6F 39`, Cmd-10 `6F 3A`, Cmd-12 `6F 3C`. The
rule is therefore `AID = 0x30 + command key number`, which lands exactly on
`core::KeyboardEncoder`'s `AID_PF8`..`AID_PF12`:

| Aid key | AID | Meaning |
|---|---|---|
| Cmd-8 | `0x38` | system graphics reset honoured; never suppressed |
| Cmd-9 | `0x39` | recoverable error, block completed |
| Cmd-10 | `0x3A` | error ended the block and graphics mode |
| Cmd-11 | `0x3B` | operator cancelled graphics |
| Cmd-12 | `0x3C` | block completed with no errors (confirmed live) |

The table prints Cmd-11 as `6F 38`, duplicating Cmd-8. That contradicts its own
Cmd-1..7 sequence and is a transcription error; `0x3B` follows the pattern but is
the one value not independently confirmed.

Errors are classified `G1`..`G5` with the split the manual's Error Handling
section gives: **G5 alone is recoverable** (show the code, sound the alarm,
resume with the next byte of the block, answer Cmd-9 at completion); **G1 to G4
terminate** both the block and graphics mode and answer Cmd-10.

Two bugs were fixed here. A graphics error used to answer nothing at all, which
stalls the host's pacing loop indefinitely *and* silently dropped the client out
of graphics mode, so the next block - which does not begin with `FF` - was fed
to the EBCDIC renderer and corrupted the alphanumeric screen. And a System
Graphics Reset answered Cmd-12 success instead of Cmd-8.

Read Status is now built from retained state by `Gddm5292Decoder::statusBytes()`
at the documented 20 bytes rather than the 21 previously sent. Caveat: the manual
gives no values for the identification and printer-type bytes and no sentinel for
"no error pending", so with no error the bytes are kept exactly as 5250ng has
always sent them, which is what GDDM accepted when it probed the device in the
capture. Only when an error *is* pending do bytes 1-2 carry the EBCDIC code and
bytes 6-7 the offset. The offset encoding is inferred as big-endian binary; no
capture has read status with an error pending.

### The plane holds colour indexes, not colours

`m_plane` is now `QImage::Format_Indexed8` with the palette in the image's colour
table. A PEL nothing has written holds index 0, so the plane is opaque black
where the picture has not been drawn - which is what the device does, its
graphics bitmap covering the whole surface. Two spec behaviours are only
expressible on an indexed plane:

- **`B3` OR and XOR combine 3-bit colour indexes.** The manual's worked example -
  red `001` XOR white `111` = turquoise `110` - is a unit test. Doing that in RGB
  gives a different answer. A PEL nothing has drawn counts as index 0 for the
  arithmetic, matching the device's cleared buffer.
- **`B4` Set Color Table is retroactive.** Redefining an index changes PELs that
  were drawn before the load, because the device stores indexes.

Qt cannot paint on an indexed image, so every draw path moved off `QPainter` onto
a Bresenham rasterizer writing indexes through one function-aware `writePel()`.
Consequences worth knowing:

- `A0` Draw Polyline now honours `B1` Set Style, which it never did. Every
  captured IBM i polyline used the solid style, so this is inert for the fixtures.
- `A3` Write Background now goes through the same write path and so honours the
  current function, as the spec requires. Its *style* is still ignored.
- Replacing Qt's line rasterizer changes **150 of 138240 PELs** when replaying
  `captures/gddm-gsarea.pcap`, all single-PEL shifts along diagonal strokes, with
  zero PELs recoloured. Neither rasterizer is authoritative - the device's own is
  - and Bresenham is the more defensible choice for this hardware. Current render:
  [`captures/gsarea-replay-indexed.png`](captures/gsarea-replay-indexed.png);
  the pre-migration render is
  [`captures/gsarea-replay-after-fill.png`](captures/gsarea-replay-after-fill.png).

Cmd-11, operator-terminated, is deliberately not implemented: nothing in the
emulator can cancel graphics from a local keyboard, so it has no producer. It is
also the one AID value the manual's own table contradicts, printing it as `6F 38`
and duplicating Cmd-8.

## Scanlines, markers and text: what GDDM emits for the rest

Captured from RSRCH09 with `GDDMTEST/GDDMPRIM` ([`GDDMPRIM.rpg`](GDDMPRIM.rpg)),
a probe that draws three markers, the string `ABCDE` in character modes 3 and 2,
and an 8x8 `GSIMG`. Artifacts:
[`captures/gddm-primitives.pcap`](captures/gddm-primitives.pcap) and
[`captures/primitives-replay-phase3.png`](captures/primitives-replay-phase3.png).

Order tally for the whole picture: `A0` 9, **`A1` 22**, **`A4` 2**, **`B5` 2**,
**`B2` 2**, `A3` 2, plus the usual control and set orders. No `A6`.

How each GDDM call lowers:

| GDDM call | Wire result |
|---|---|
| `GSCHAR` with `GSCM` 3 (vector, the display default) | `A0` Draw Polyline |
| `GSCHAR` with `GSCM` 2 (image symbols) | `A1` Draw Scanline, one per glyph row |
| `GSIMG` | `A1` Draw Scanline, one per image row |
| `GSMS` 1 then `GSMARK` | `B5` marker 4 (cross) + `A4` |
| `GSMS` 4 then `GSMARK` | `B5` marker 2 (empty box) + `A4` |
| `GSMS` 8 then `GSMARK` | no marker order at all: rasterized host-side into five `A1` scanlines |

So GDDM uses the device marker orders when a device shape matches and falls back
to scanlines when it does not, the same "host-side rasterization when the device
cannot express it" pattern seen with area fills.

### `A1` Draw Scanline encoding, settled by capture

Bytes 1-4 are the X and Y of the leftmost PEL. Every following graphics-data
byte carries **six** pattern bits, **most significant bit leftmost**, advancing
in +X; a 1 sets the PEL with the current colour and function, a 0 leaves it
alone. An 8-PEL row therefore arrives as two data bytes with the second padded
with zeros - which is why the 8x8 image produced eight orders of six data bytes.

The probe made this unambiguous rather than inferred. Its first image row was
EBCDIC `'A'` (`0xC1` = `11000001`) and the rest blanks (`0x40` = `01000000`).
The capture shows row 0 as `70 50`, whose six-bit halves concatenate to
`110000010000` - the expected eight bits followed by four zero pad bits - and
the remaining rows as `50 40` = `010000000000`. Rows arrive top first, in
*decreasing* device Y.

### Markers

The coordinate is the **centre** of a 5x5 box. That is confirmed independently:
GDDM's own scanline fallback for marker 8 placed its top-left at centre minus
two, landing in exactly the same footprint the `A4` markers occupy.

The nine device shapes are described in words in the Functions Reference and its
glyph column does not survive text extraction - but the page renders fine as an
image. Rendering PDF page 139 at 300 dpi
([`research/marker-shapes-page-2-87.png`](research/marker-shapes-page-2-87.png))
gives the glyphs directly, and **all nine shapes in `kMarkerShapes` match them**:
solid and empty boxes, plus, cross, solid and hollow diamonds, and the three-
and four-segment asterisks.

The same page also settles the field width: byte 2 is laid out `0100aaaa`, so the
marker is the low **four** bits, not the whole six-bit payload.

A probe calling `GSMS` 0 through 8 (`GDDMMARK.rpg`) shows GDDM using **seven** of
the nine device markers - 2, 3, 4, 5, 6, 7 and 8 - so the shapes matter more than
the first capture suggested. The mapping is not identity: `GSMS` 0 and 1 both go
to device marker 4, 2 to 3, 3 to 6, 4 to 2, 5 to 7, 6 to 8, 7 to 5, and `GSMS` 8
falls back to host-side scanlines as before.

### `B2` Set Style Offset

Bits `aa` select which of the four Set Style runs a line begins on
(`00`..`11` = 1st..4th) and the low four bits override that run's length, zero
meaning keep it. IBM i emitted only `40`, meaning segment 0 with no override,
so the nonzero behaviour is implemented from the manual but unobserved.

### Implementation notes

`A1`, `A4`, `B5` and `B2` are implemented, and `A3` Write Background now honours
the current style as well as the function. `A6` stays parsed-but-not-rendered:
still nothing has ever emitted it.

This gives Phase 1's recoverable-error path its first real producer. A marker
whose box will not fit on the surface raises **G5**, the one recoverable
graphics error: the device skips that marker, carries on with the rest of the
block, keeps graphics mode, and answers Cmd-9 instead of Cmd-10.

## Live validation against RSRCH09

Run 2026-08-25 with the rebuilt client (`--enable-mcp-server`), device type
`IBM-5292-2`, capture started after sign-on. Artifacts:
[`captures/gddm-live-phase123.pcap`](captures/gddm-live-phase123.pcap),
[`captures/live-gsarea-phase123.png`](captures/live-gsarea-phase123.png),
[`captures/live-primitives-phase123.png`](captures/live-primitives-phase123.png).

Confirmed live:

- `GDDMSHAF` renders all six shapes, including the three `A5` fills that were
  previously absent. Matches the offscreen replay.
- `GDDMPRIM` renders all four groups: three markers, both `ABCDE` strings
  (mode 3 via `A0`, mode 2 via `A1`), and the `GSIMG` bit pattern.
- 23 graphics blocks, 23 pacing responses, **zero decoder errors**.
- Read Status still satisfies GDDM. The field arrives as
  `ff ff ff f2 80 ff ff` followed by blanks, i.e. the previously validated
  header bytes with "no error pending" in both the code and offset fields. Note
  the 21-to-20 byte length correction is not observable here: byte 21 was a
  blank and the surrounding screen is blanks too.
- Replaying the live capture offscreen reproduces exactly 23 blocks and 23
  Cmd-12 responses, so the live client and the replay harness agree.

### The error paths are not reachable through GDDM

Every one of the 28 responses across both captures was Cmd-12. An attempt to
provoke the recoverable `G5` with `GDDMEDGE.rpg` ([`GDDMEDGE.rpg`](GDDMEDGE.rpg)),
which places a marker at world x=0 so its 5x5 box would run off the surface,
showed why: **GDDM clips host-side and never asks the device to draw an
out-of-bounds marker.** For the clipped marker it emitted five `A1` scanlines
of the partially visible cross instead of an `A4`, and sent `A4` only for the
in-bounds marker at (239,143). Capture:
[`captures/gddm-live-marker-clipping.pcap`](captures/gddm-live-marker-clipping.pcap),
render [`captures/live-g5-edge.png`](captures/live-g5-edge.png) - where the
partial cross on the left edge is correct, being GDDM's own rasterization.

The same reasoning applies to `G1`..`G4` and Cmd-10: IBM i GDDM generates
well-formed streams, so the client's error handling is reachable only from a
corrupted stream, a different host, or a fuzzer. It still matters - without it a
malformed block hangs the pacing loop and corrupts the alphanumeric plane - but
it cannot be validated against this host. Validating Cmd-9 and Cmd-10 on the
wire would need a host program that writes the 5250 data stream directly rather
than going through GDDM.

## Plane layering and the graphics-mode indicator

The widget used to paint the alphanumeric cells first and composite the graphics
plane over them. That is backwards: the GDDM guide describes the picture as
showing "as a background to any alphanumeric data". Worse, it was not a subtle
ordering nit. `GDDMSHAF` issues `A3` Write Background in colour 0, so the whole
plane is *opaque* black - which meant the plane hid every character on the
screen. Compare
[`captures/live-gsarea-phase123.png`](captures/live-gsarea-phase123.png), where
the Read Status text is buried, with
[`captures/live-phase4-layering.png`](captures/live-phase4-layering.png), where
it reads normally.

The fix is two parts, both in `Q5250ScreenWidget::renderScreen`/`renderCell`:

- the plane is drawn *before* the cell loop; and
- a cell skips its default background fill while the plane is visible, so the
  picture shows through. A cell carrying its own background - reverse video, for
  instance - still fills, or its text would be illegible against the graphics.

The 5292's blue uppercase `G` graphics-display-mode indicator is now drawn in the
bottom-left cell whenever the plane is visible.

**Reduced line spacing is deliberately not implemented.** The device compresses
its text rows in graphics mode so that 25 rows and the 288-PEL graphics area both
fit a fixed-height CRT. 5250ng has no such constraint: it scales the plane into
the text area with the aspect ratio preserved, so the two planes already
coincide. Compressing the text would shrink it for no benefit and would break
that correspondence rather than improve it. If strict device fidelity is wanted
later it belongs behind a user option, not as default behaviour.

## Diagnostics and fuzzing

### Diagnostics, not rejections

Two things are reported without ever failing a block, because the research above
warns against rejecting on either:

- **Block length.** The 5292 Functions Reference documents 11 to 256 bytes; the
  IBM i GDDM guide allows up to 1920. Both bounds only produce a warning. IBM i
  was observed using blocks up to 252 bytes, so it respects the device limit,
  and the long runs of `93` padding exist precisely to reach the 11-byte floor.
- **Coordinates outside the 480x288 surface** are rejected as `G1`, which ends
  the block. The manual defines no error code for them, so this is a choice
  rather than a requirement; it is safe in practice because GDDM clips
  host-side and has never been observed sending one.
- **Suppressed pacing.** `Result::pacingSuppressed` distinguishes "deliberately
  silent" from "silent by mistake". That distinction is what makes the
  always-answer property checkable.

`Result` also carries `warning` and `outOfRangeCoordinates`, and the decoder
exposes `lastOrder()` and `lastBlock()`. The command handler emits one greppable
line per block with block number, size, mode, display state, last order and the
AID sent, plus the raw block hex when a block failed.

`A0` was consolidated onto the shared `decodePoints` helper so that every
coordinate order validates its input the same way.

### Fuzzing

`tests/unit/test_gddm_5292_fuzz.cpp` mutates the real captured blocks from a
fixed seed - bit flips, byte substitutions, truncations, splices, tail
duplication - and asserts the properties a corrupted or hostile stream could
break:

1. No crash and no out-of-bounds write.
2. **A handled block always answers**, with a Graphic Aid key code or an explicit
   suppression. This is the Phase 1 property and the regression most worth
   guarding: a silent block stalls the host's pacing loop indefinitely.
3. A nonrecoverable error leaves graphics mode terminated; a recoverable one
   answers Cmd-9.
4. A reported error offset lies inside the block.
5. The plane keeps its geometry and indexed format.
6. After a nonrecoverable failure a well-formed block still decodes, so a
   failure cannot poison the decoder.

Three cases: mutants against a fresh decoder, mutants against one long-lived
decoder so they land on arbitrary carried state, and hand-written adversarial
blocks (every possible order byte alone, 600-byte runs of a single control byte,
coordinates at the 10-bit maximum, a polygon 400 edges past the `G4` limit, a
scanline pattern wider than the surface).

The suite passes clean, including under `-fsanitize=address`. That it passed
first time is not self-evidently meaningful, so it was verified to have teeth:
removing the X-axis clip from `writePel` makes ASAN report a
heap-buffer-overflow in `writePel` reached from `drawScanline`, caught by the
over-wide scanline case.

Not covered: the fuzzer is decoder-level. The handler-level property that
graphics bytes never reach the EBCDIC renderer is exercised by
`tests/unit/test_command_handler_soh.cpp`, not by mutation.

## Display rendering architecture for 5250ng

Recommended separation:

```text
TN5250 GDS decoder
  -> WTD command handler
      -> graphics block classifier/state machine
          -> 5292 graphics-order decoder
              -> retained GraphicsState + 480x288 raster layer
                  -> terminal widget composites graphics and A/N planes
      -> ordinary TN5250StreamRenderer (only for non-graphics WTD data)
```

Suggested state:

```cpp
struct Gddm5292State {
    bool graphicsMode = false;
    bool displayEnabled = false;
    bool suppressPacing = false;
    std::optional<PendingVariableOrder> pending;
    std::array<QColor, 8> palette;
    int colorIndex = 7;
    LineStyle style = solidStyle();
    int styleOffset = 0;
    RasterFunction function = RasterFunction::Replace;
    Marker marker = Marker::Solid5;
    int lineWeight = 1;
    FillMode fillMode;
    QImage pixels{480, 288, QImage::Format_ARGB32_Premultiplied};
};
```

Implementation rules:

- Classify at the beginning of the WTD display payload, before EBCDIC conversion.
- If `graphicsMode` is false, only leading `FF` starts graphics.
- If `graphicsMode` is true, route the whole following WTD payload to the graphics decoder until `95`/reset/error.
- `90` commits the block but preserves mode and does not terminate a pending nonspanned semantic state.
- `91` preserves the current variable order so its data resumes in the next block.
- `92` completes a variable order and executes it.
- Render to a fixed 480x288 logical surface first; scale at paint time with aspect ratio preserved.
- Use nearest-neighbor sampling for fidelity unless a user option requests smoothing.
- Keep the alphanumeric `ScreenBuffer` unchanged. Graphics are a second layer with independent visibility.
- Implement OR/XOR/Replace per PEL/color-index semantics. For full fidelity, keep an 8-bit palette-index raster rather than only RGBA pixels; this makes later color-table changes and XOR deterministic.
- Double-weight lines and patterned styles should operate in device PEL space, not widget pixels.
- Polygon fill uses the selected reference direction/style and has a 128 nonhorizontal-edge limit.
- Draw Scanline consumes its post-coordinate data as one PEL bit per low-six-bit graphics-data payload, in documented bit order; validate this with the manual diagrams and a capture.
- Do not send the pacing AID until the complete block has been decoded and committed to the raster.
- Expose graphics-mode, last-order, last-error, block count, and last raw block in debug logs.

Current code touchpoints in the 5250ng repository:

- `src/ui/rendering/tn5250_command_handler.cpp`: receives raw WTD display data and owns response sending.
- `src/ui/rendering/tn5250_stream_renderer.cpp`: currently treats every byte `>= 0x40`, including a leading `FF`, as printable EBCDIC. Graphics must branch before this loop.
- `src/ui/widgets/Q5250ScreenWidget/events.cpp`: existing AID-response construction can be reused for pacing.
- `src/core/keyboard_encoder.h`: PF12 AID is `0x3C`.
- `src/session/config.*` and TN5250 worker/client configuration: already carry `deviceType`.
- `src/network/tn5250/tn5250/devices/devices.cpp`: already lists `IBM-5292-2`.
- `src/mcp/McpToolHandler.cpp` and `src/agent/tool_definitions.h`: `create_session` currently exposes only host, port, and TLS.
- `src/ui/main_window.cpp`: MCP-created sessions use a default `SessionConfig`, currently `IBM-3179-2`.

## Negotiation change required

The smallest test-enabling change is to add an optional `deviceType` to the MCP `create_session` schema and propagate it:

```json
{
  "hostname": "192.0.2.10",
  "port": 23,
  "useTLS": false,
  "deviceType": "IBM-5292-2"
}
```

Then set `SessionConfig::deviceType` before `connectToServer`. Also consider an optional requested IBM i virtual device name for reproducible tests.

Test sequence after this change:

1. Connect as `IBM-5292-2`.
2. Sign on and run `DSPJOB`; verify the device/job identity.
3. Call `GDDMTEST/GDDMDEMO`.
4. Capture raw host-to-client GDS records and client pacing replies.
5. Confirm that the host no longer logs dummy `5292M2` substitution.
6. Confirm the first graphics WTD payload byte is `FF`.
7. Confirm exact TCP/TN5250 success response generated by an automatic PF12-style AID.
8. Expand the sample to set colors, draw a rectangle and filled polygon, and write vector text.

If IBM i rejects `IBM-5292-2` as a TN5250 terminal type, emulate the capability negotiation performed by the historical `GR5250` workstation program instead of merely changing the terminal-type string. The IBM guide explicitly required `GR5250` to be run in DOS before starting GDDM under old PC 5250 emulation. A rejected terminal type or continued dummy substitution is evidence that the graphics capability is established out of band rather than solely by TELNET TERMINAL-TYPE.

## The printer orders, and what "screen copy" means here

### The mystery `C3` is solved

Every GDDM picture's opening block carries a `C3` with 14 data bytes, previously
noted here as unexplained. It is **Load Printer Graphics Color Mix Table**: seven
index/bcmy pairs, one per writable graphics colour index, mapping the display
palette onto the printer's four inks (black, cyan, magenta, yellow) so that a
screen copy prints colours matching the picture. Decoding the captured bytes
against the `B4` palette loaded in the same block gives an exact correspondence:

| Index | bcmy | Printer colour | Display colour GDDM set |
|---:|---|---|---|
| 1 | 0110 | cyan+magenta | blue |
| 2 | 0011 | magenta+yellow = red | red |
| 3 | 0010 | magenta | magenta |
| 4 | 0101 | cyan+yellow = green | green |
| 5 | 0100 | cyan | cyan |
| 6 | 0001 | yellow | yellow |
| 7 | 1000 | black | white |

Every entry is the subtractive equivalent of its display colour, with white
becoming black - the inversion you want for ink on white paper. GDDM sets this up
at picture start so a screen copy is correct whenever it happens.

### What is implemented

All five printer orders are now decoded rather than dropped, and their state is
retained rather than acted on, because 5250ng has no attached printer:

- `C0` Printer Data Follows: payload retained, bounded.
- `C1` Screen Copy: reported, see below.
- `C2` Load Printer A/N Color Mix Table: index/bcmy pairs into a 32-entry table,
  defaults from the manual's chart. Indexes 7, 15, 23 and 31 are fixed in the
  device, and attempting them raises **P5** - the one printer error an emulator
  can raise on its own, P1 to P4 being physical printer conditions. P5 is
  nonrecoverable, so it ends the block with Cmd-10.
- `C3` Load Printer Graphics Color Mix Table: as above, 8-entry table.
- `C4` Set Printer Time-Out: retained, one unit being 5.5 seconds.

### Screen copy deliberately writes no file

`Q5250ScreenWidget::compositeScreenImage()` and `exportCompositeScreen(path)`
render the composite - alphanumeric plane with the graphics plane behind it -
because that combination exists nowhere else. Nothing host-side can serialise the
graphics plane, which is exactly why path 1 below is the client's job.

`C1` does **not** trigger that export. The decoder reports
`Result::screenCopyRequested`, the handler logs it and emits
`TN5250CommandHandler::screenCopyRequested()`, and the application decides. Doing
otherwise would hand a remote host an unprompted file-write primitive on the
user's machine, which is a poor trade for fidelity to an order the real device
only supported "under a printer RPQ" anyway. Connecting that signal to a menu
action or a configured export directory is the remaining UI work.

## Printing: three distinct paths

“Printing GDDM graphics” can mean three different mechanisms.

### 1. 5292 local screen copy

The graphics stream has I/O feature orders (`C0..C4`) for the printer attached to a physical 5292. `C1` requests screen copy. Emulating this faithfully means compositing A/N plus the graphics bitmap and sending it to a local print/export backend. This is not an ACS TN5250 printer session.

### 2. GDDM output directly to an IBM i printer device/spool file

The application opens a printer with `DSOPEN`, makes it current with `DSUSE`, draws for that device, and forces the page. GDDM opens `QSYS/QPGDDM` by default. Its historical defaults are SCS, 99x132 page, 9 LPI, 10 CPI, overflow 90, spooled output, and a 13.2 by 10 inch graphics area.

Supported historical printers include 3812, 3816, 4028, 4214, 4224, 4234-2, 5224, and 5225. Output may be SCS or IPDS and is device-token dependent. A spooled graphic generated for one printer type is generally not portable to another.

In this path, the display emulator does not render the print stream. An IBM i printer session, Host Print Transform, IPDS/AFP conversion, or a real printer consumes it.

### 3. Copy a displayed GDDM picture to an alternate GDDM printer

The application opens/uses an alternate printer and invokes `GSCOPY`, `FSCOPY`, or `DSCOPY`. For best output, IBM recommends making the printer the primary current device and redrawing, because printer resolution, colors, page grid, symbols, and area patterns differ from the display.

Important limitations:

- printer/plotter default character mode is image-symbol mode 2; display default is vector-symbol mode 3;
- display-file alphanumeric fields are not reproduced by ordinary GDDM printer/plotter output;
- printer color mixing is overpaint and modified display color tables are not reproduced literally;
- area-fill patterns are finer on printer/plotter output;
- `ASREAD`/`FSFRCE` eject a printer page.

The ordinary 5250 Host Print key prints the character presentation space via IBM i. It does not magically serialize the separate 5292 graphics plane. 5250ng should offer an explicit “print/export composite screen” action if users need the rendered GDDM picture.

## ACS answer in practical terms

- ACS supports standard TN5250 display sessions and TN5250 printer sessions.
- ACS can download/view IBM i spool files and can print transformed SCS/AFP data.
- ACS does not advertise or emulate `IBM-5292-2` for a display session.
- ACS does not provide the historical DOS `GR5250` graphics function.
- Therefore ACS will not display this IBM i GDDM/5292 graphics stream in a 5250 session.
- IBM Personal Communications has explicit host-graphics support, but that should not be generalized to ACS, and its documented GDDM material is commonly oriented to 3270/mainframe host graphics.

## Test plan

### Decoder unit tests

- leading `FF` classification and no EBCDIC fall-through;
- single block ending `95`;
- multiblock nonspanned `90`;
- variable order split by `91`, resumed next WTD, completed by `92`;
- System Graphics Reset `FF FF`;
- palette defaults and Set Color/Table;
- exact coordinate decoding for IBM's `(0,10),(100,50),(150,25)` example;
- line style/offset, double weight, OR/XOR/Replace;
- scanline bit pattern;
- polygon closure/fill and 128-edge rejection;
- display on/off without deleting the retained raster;
- pacing response emitted exactly once after commit;
- order `96` suppresses normal pacing response;
- malformed data produces a graphics error and never corrupts the A/N screen.

### Integration tests on RSRCH09

1. Existing `GDDMDEMO`: diagonal line and ASREAD.
2. Color test: seven indexed lines and a redefined color-table entry.
3. Style test: solid/dashed, single/double, OR/XOR/Replace overlaps.
4. Area test: outlined and filled polygons plus shield area.
5. Scanline/image test: known 16x13 bit pattern from the IBM GDDM guide.
6. Mixed-plane DDS test using `ALWGPH`.
7. Multipage/multiblock test large enough to force pacing.
8. Graphics display off/on retention test.
9. Reset and malformed-order recovery test.
10. Print/export composite test, separate from IBM i spooled-printer output.

Capture every integration session to PCAP after sign-on so credentials are not recorded. Add sanitized captures as replay fixtures; 5250ng already has PCAP replay support.

## Immediate next steps

Done as of 2026-08-25:

1. ~~Add MCP `deviceType` support and try `IBM-5292-2` against RSRCH09.~~ Accepted directly by IBM i.
2. ~~Capture and preserve the first real GDDM exchange before writing broad renderer code.~~ See `captures/`.
3. ~~Implement the graphics block classifier and pacing response with a raw-block debug dump.~~ 13/13 blocks paced correctly.
4. ~~Implement the control/set orders and `A0` polyline first.~~ Confirmed against a live picture.
5. ~~Add a 480x288 indexed graphics plane and composite it in the terminal widget.~~

Remaining:

6. ~~Implement `A5` Fill Polygon with an even-odd scanline rasterizer.~~ Done; `GDDMSHAF` now renders in full.
7. ~~Error classification, fatal/recoverable split, Graphic Aid key codes, and a real Read Status.~~ Done.
8. ~~Apply `B1` Set Style to `A0` Draw Polyline as well.~~ Done as part of the indexed-plane rasterizer.
9. ~~Honour `B3` Set Function OR/XOR/Replace, and make `B4` retroactive.~~ Done on the indexed plane.
10. ~~Capture `GSCHAR`/`GSCM`, `GSIMG`, `GSMARK` to find out which of `A1`, `A4`, `B5` IBM i actually emits.~~ Done; all three are real. See above.
11. ~~Implement `A1` Draw Scanline, `B5` Set Marker plus `A4` Write Polymarker with the recoverable `G5` bounds check, `B2` Set Style Offset, and style on `A3`.~~ Done.
11a. Still uncaptured: a `GSMIX` sweep to see whether IBM i ever leaves Replace, and `GSPAT` 1-16 to map which patterns use a styled `A5` versus host rasterization. Neither blocks anything; both would firm up untested paths.
12. ~~Fix the plane/text layering, and add the blue `G` indicator.~~ Done. Reduced line spacing was deliberately skipped; see above.
13. ~~Block length and coordinate range diagnostics, and decoder fuzzing.~~ Done.
14. Decide whether `A6` Define Shield Area is worth implementing. GDDM never emitted it in this capture; parse it defensively but deprioritize rendering it.
15. ~~Investigate the unexplained `C3` order with 14 data bytes in GDDM's opening block.~~ Solved: it is the printer graphics colour-mix table. See above.
16. ~~Composite screen print/export, and decode the printer orders.~~ Done. `C1` reports a request rather than writing a file; wiring that signal to a menu action or configured directory is the remaining UI work.
17. Surface the per-block diagnostics in the UI as well as the log; only the log side is done.
18. Not implemented and not planned: the IEEE-488 feature orders (`01`-`03`, `E0`-`E2`) for plotter attachments. They reach the decoder's undefined-order path as `G2`, which is correct for a device with no IEEE feature.

