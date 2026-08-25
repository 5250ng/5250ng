#!/usr/bin/env python3
"""Decode the IBM 5292 GDDM graphics order stream out of a TN5250 capture.

Usage:
    tshark -r capture.pcap -Y 'tcp.srcport==23 && tcp.len>0' \
        -T fields -e tcp.payload | tr -d ':\n' > h2c.hex
    python3 decode_gddm_capture.py h2c.hex

Strips the telnet layer, splits the host->client stream into GDS records, and
walks the 5292 graphics orders. Graphics mode and any variable order split by
More-Data-to-Come (0x91) are carried across record boundaries, which is
required: GDDM ends most blocks with 0x90, so only the first block of a
picture starts with 0xFF.
"""

import sys

raw = bytes.fromhex(open(sys.argv[1]).read().strip())

# --- strip telnet: FF FF -> literal FF, FF EF -> record boundary ---
records, cur, i = [], bytearray(), 0
while i < len(raw):
    b = raw[i]
    if b == 0xFF and i + 1 < len(raw):
        nxt = raw[i + 1]
        if nxt == 0xFF:
            cur.append(0xFF); i += 2; continue
        if nxt == 0xEF:
            records.append(bytes(cur)); cur = bytearray(); i += 2; continue
        if nxt in (0xFA, 0xFB, 0xFC, 0xFD, 0xFE):
            i += 3; continue
        i += 2; continue
    cur.append(b); i += 1
if cur:
    records.append(bytes(cur))

FIXED = {0xB0: ("Set Color", 1), 0xB1: ("Set Style", 4), 0xB2: ("Set Style Offset", 1),
         0xB3: ("Set Function", 1), 0xB5: ("Set Marker", 1), 0xB6: ("Set Line Weight", 1),
         0xB7: ("Set Fill Mode", 2), 0xA3: ("Write Background", 1),
         0x80: ("Read Status", 2), 0xC4: ("Set printer timeout", 1)}
VAR = {0xB4: "Set Color Table", 0xA0: "Draw Polyline", 0xA1: "Draw Scanline",
       0xA4: "Write Polymarker", 0xA5: "Fill Polygon", 0xA6: "Define Shield Area",
       0xC0: "Printer Data Follows", 0xC2: "Load printer A/N color-mix",
       0xC3: "Load printer gfx color-mix"}
CTRL = {0x90: "End Block (mode stays active)", 0x91: "More Data to Come",
        0x92: "End of Data", 0x93: "Graphics Display On",
        0x94: "Graphics Display Off", 0x95: "End Graphics",
        0x96: "Suppress Pacing Response", 0xC1: "Screen Copy"}

def pts(payload):
    v = [((payload[j] & 0x0F) << 6) | (payload[j + 1] & 0x3F)
         for j in range(0, len(payload) - 1, 2)]
    return list(zip(v[0::2], v[1::2]))

gfx = False           # graphics mode persists across records
pending = None        # (order_byte, accumulated_data) split by 0x91
tally = {}
blocks = 0

for rn, rec in enumerate(records):
    k = rec.find(b"\x04\x11")
    if k < 0:
        continue
    data = rec[k + 4:]
    if not data:
        continue
    if not gfx:
        if data[0] != 0xFF:
            continue                       # ordinary alphanumeric WTD
        gfx = True
        data = data[1:]
        lead = "FF (Begin Graphics) "
    else:
        lead = "(graphics mode already active) "
    blocks += 1
    print("=== record %d / graphics block %d: %s%d bytes ===" % (rn, blocks, lead, len(data)))
    p = 0
    if pending is not None:
        ob, acc = pending
        q = p
        while q < len(data) and (data[q] & 0xC0) == 0x40:
            q += 1
        acc = acc + data[p:q]
        if q < len(data) and data[q] == 0x92:
            print("    ..     %s RESUMED: %d total data bytes, points=%s"
                  % (VAR[ob], len(acc), pts(acc) if ob in (0xA0,0xA4,0xA5,0xA6) else "-"))
            tally["%s (spanned)" % VAR[ob]] = tally.get("%s (spanned)" % VAR[ob], 0) + 1
            pending = None
            p = q + 1
        else:
            pending = (ob, acc)
            p = q + 1
    while p < len(data):
        b = data[p]
        if b in CTRL:
            run = 1
            while b in (0x93, 0x94, 0x90) and p + run < len(data) and data[p + run] == b:
                run += 1
            tally[CTRL[b]] = tally.get(CTRL[b], 0) + run
            print("    %02X%s %s" % (b, (" x%-3d" % run) if run > 1 else "    ", CTRL[b]))
            p += run
            if b == 0x95:
                if p < len(data):
                    print("    ...   %d trailing bytes: %s" % (len(data) - p, data[p:].hex()))
                gfx = False
                break
            continue
        if b in FIXED:
            name, n = FIXED[b]
            payload = data[p + 1:p + 1 + n]
            tally[name] = tally.get(name, 0) + 1
            print("    %02X     %s: %s -> 6-bit %s"
                  % (b, name, payload.hex(), [x & 0x3F for x in payload]))
            p += 1 + n
            continue
        if b in VAR:
            q = p + 1
            while q < len(data) and (data[q] & 0xC0) == 0x40:
                q += 1
            payload = data[p + 1:q]
            term = data[q] if q < len(data) else None
            tally[VAR[b]] = tally.get(VAR[b], 0) + 1
            extra = ""
            if b in (0xA0, 0xA4, 0xA5, 0xA6):
                extra = " points=%s" % (pts(payload),)
            elif b == 0xB4:
                cols = []
                for j in range(0, len(payload) - 2, 3):
                    idx, rg, bl = payload[j] & 7, payload[j + 1], payload[j + 2]
                    cols.append((idx, (rg >> 3) & 7, rg & 7, (bl >> 3) & 7))
                extra = " (index,r,g,b)=%s" % (cols,)
            print("    %02X     %s: %d data bytes term=%s%s"
                  % (b, VAR[b], len(payload), "%02X" % term if term else "none", extra))
            if term == 0x91:
                pending = (b, payload)
                print("         ^ split by More-Data-to-Come, resumes next block")
                p = q
            else:
                p = q + (1 if term == 0x92 else 0)
            continue
        print("    %02X     *** UNRECOGNIZED ***" % b)
        p += 1
    print()

print("graphics blocks: %d" % blocks)
print("order tally:")
for k2, v in sorted(tally.items(), key=lambda x: -x[1]):
    print("   %-32s %d" % (k2, v))
